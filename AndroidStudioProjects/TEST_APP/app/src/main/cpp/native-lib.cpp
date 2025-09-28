#include <jni.h>
#include <string>
#include <vector>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <fstream>
#include <limits> // std::numeric_limits 사용
#include <algorithm> // std::min 사용

#include "llama.h"

#define TAG "LlamaCpp_JNI"

static llama_model *g_model = nullptr;
static llama_context *g_ctx = nullptr;
static bool g_model_loaded_successfully = false;

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_test_1app_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    if (g_model_loaded_successfully && g_ctx != nullptr) {
        std::string hello = "Llama.cpp model loaded. Ready for inference!";
        return env->NewStringUTF(hello.c_str());
    }
    std::string hello = "Llama.cpp model not loaded yet.";
    return env->NewStringUTF(hello.c_str());
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_test_1app_MainActivity_loadModelJNI(
        JNIEnv *env,
        jobject /* this */,
        jobject java_asset_manager,
        jstring model_file_name_in_assets,
        jstring app_cache_dir_path
) {
    if (g_model_loaded_successfully) {
        __android_log_print(ANDROID_LOG_INFO, TAG, "Model already loaded.");
        return JNI_TRUE;
    }

    g_model_loaded_successfully = false;

    const char *model_asset_name_c_str = env->GetStringUTFChars(model_file_name_in_assets, nullptr);
    if (!model_asset_name_c_str) return JNI_FALSE;
    std::string model_asset_name(model_asset_name_c_str);
    env->ReleaseStringUTFChars(model_file_name_in_assets, model_asset_name_c_str);

    const char *cache_dir_path_c_str = env->GetStringUTFChars(app_cache_dir_path, nullptr);
    if (!cache_dir_path_c_str) return JNI_FALSE;
    std::string cache_dir(cache_dir_path_c_str);
    env->ReleaseStringUTFChars(app_cache_dir_path, cache_dir_path_c_str);

    std::string model_path_internal = cache_dir + "/" + model_asset_name;
    __android_log_print(ANDROID_LOG_INFO, TAG, "Internal model path: %s", model_path_internal.c_str());

    AAssetManager *asset_manager = AAssetManager_fromJava(env, java_asset_manager);
    if (!asset_manager) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to get AAssetManager.");
        return JNI_FALSE;
    }

    std::ifstream check_file(model_path_internal);
    bool file_exists = check_file.good() && check_file.peek() != EOF;
    check_file.close();

    if (!file_exists) {
        __android_log_print(ANDROID_LOG_INFO, TAG, "Model file not found in cache. Copying from assets...");
        AAsset *asset = AAssetManager_open(asset_manager, model_asset_name.c_str(), AASSET_MODE_STREAMING);
        if (!asset) {
            __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to open asset: %s", model_asset_name.c_str());
            return JNI_FALSE;
        }
        std::ofstream out_file(model_path_internal, std::ios::binary);
        if (!out_file.is_open()) {
            __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to open internal storage: %s", model_path_internal.c_str());
            AAsset_close(asset);
            return JNI_FALSE;
        }
        char buffer[1024];
        int bytes_read;
        while ((bytes_read = AAsset_read(asset, buffer, sizeof(buffer))) > 0) {
            out_file.write(buffer, bytes_read);
        }
        out_file.close();
        AAsset_close(asset);
        __android_log_print(ANDROID_LOG_INFO, TAG, "Model copied successfully.");
    } else {
        __android_log_print(ANDROID_LOG_INFO, TAG, "Model file already exists in cache.");
    }

    llama_backend_init();
    llama_model_params model_params = llama_model_default_params();
    g_model = llama_model_load_from_file(model_path_internal.c_str(), model_params);
    if (!g_model) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to load model.");
        llama_backend_free();
        return JNI_FALSE;
    }
    __android_log_print(ANDROID_LOG_INFO, TAG, "Model loaded successfully.");

    llama_context_params ctx_params = llama_context_default_params();
    // ctx_params.n_ctx = 2048; 
    // ctx_params.n_threads = std::max(1, (int)std::thread::hardware_concurrency() / 2);
    // ctx_params.n_batch = 512; 

    g_ctx = llama_init_from_model(g_model, ctx_params);
    if (!g_ctx) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to create context.");
        llama_model_free(g_model);
        g_model = nullptr;
        llama_backend_free();
        return JNI_FALSE;
    }
    __android_log_print(ANDROID_LOG_INFO, TAG, "Context created successfully.");
    g_model_loaded_successfully = true;
    return JNI_TRUE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_test_1app_MainActivity_unloadModelJNI(
        JNIEnv *env,
        jobject /* this */) {
    if (g_ctx) {
        llama_free(g_ctx);
        g_ctx = nullptr;
        __android_log_print(ANDROID_LOG_INFO, TAG, "Context freed.");
    }
    if (g_model) {
        llama_model_free(g_model);
        g_model = nullptr;
        __android_log_print(ANDROID_LOG_INFO, TAG, "Model freed.");
    }
    llama_backend_free();
    __android_log_print(ANDROID_LOG_INFO, TAG, "Llama backend freed.");
    g_model_loaded_successfully = false;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_test_1app_MainActivity_generateTextJNI(
        JNIEnv *env,
        jobject /* this */,
        jstring prompt_str) {

    if (!g_model_loaded_successfully || !g_ctx || !g_model) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Model not loaded, cannot generate text.");
        return env->NewStringUTF("Error: Model not loaded.");
    }

    const char *prompt_c_str = env->GetStringUTFChars(prompt_str, nullptr);
    if (!prompt_c_str) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to get prompt string from Java.");
        return env->NewStringUTF("Error: Could not get prompt.");
    }
    std::string prompt(prompt_c_str);
    env->ReleaseStringUTFChars(prompt_str, prompt_c_str);

    __android_log_print(ANDROID_LOG_INFO, TAG, "Starting inference with prompt: %s", prompt.c_str());

    llama_memory_t mem = llama_get_memory(g_ctx);
    if (mem) {
        llama_memory_seq_rm(mem, 0, 0, -1); // Clear KV cache for sequence 0
    } else {
        __android_log_print(ANDROID_LOG_WARN, TAG, "Failed to get memory from context for KV cache clear.");
    }

    const struct llama_vocab *vocab = llama_model_get_vocab(g_model);
    if (!vocab) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to get model vocab.");
        return env->NewStringUTF("Error: Failed to get model vocab.");
    }

    std::vector<llama_token> prompt_tokens;
    prompt_tokens.resize(llama_n_ctx(g_ctx)); // Max possible size for tokenization buffer

    int n_prompt_tokens = llama_tokenize(
        vocab,
        prompt.c_str(),
        prompt.length(),
        prompt_tokens.data(),
        prompt_tokens.size(),
        true,  // add_special (BOS/EOS)
        false  // parse_special
    );

    if (n_prompt_tokens < 0) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to tokenize prompt (buffer too small or error).");
        return env->NewStringUTF("Error: Failed to tokenize prompt.");
    }
    prompt_tokens.resize(n_prompt_tokens);

    if (n_prompt_tokens >= llama_n_ctx(g_ctx) - 4) { // llama_n_ctx gives context size
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Prompt is too long (%d tokens, max %d).", n_prompt_tokens, llama_n_ctx(g_ctx) - 4);
        return env->NewStringUTF("Error: Prompt too long.");
    }

    if (n_prompt_tokens > 0) {
        llama_batch prompt_batch = llama_batch_init(n_prompt_tokens, 0, 1); // embd=0 for token IDs, n_seq_max=1
        if (!prompt_batch.token) {
            __android_log_print(ANDROID_LOG_ERROR, TAG, "llama_batch_init failed for prompt batch.");
            return env->NewStringUTF("Error: Prompt batch init failed.");
        }

        for (int i = 0; i < n_prompt_tokens; ++i) {
            prompt_batch.token[i]    = prompt_tokens[i];
            prompt_batch.pos[i]      = i;
            prompt_batch.n_seq_id[i] = 1;
            if (prompt_batch.seq_id && prompt_batch.seq_id[i]) {
                 prompt_batch.seq_id[i][0] = 0; // Sequence ID 0
            } else {
                __android_log_print(ANDROID_LOG_ERROR, TAG, "Prompt batch seq_id structure not allocated as expected.");
                llama_batch_free(prompt_batch);
                return env->NewStringUTF("Error: Prompt batch seq_id allocation issue.");
            }
            prompt_batch.logits[i]   = false;
        }
        prompt_batch.n_tokens = n_prompt_tokens;
        prompt_batch.logits[prompt_batch.n_tokens - 1] = true; // Request logits for the last token

        if (llama_decode(g_ctx, prompt_batch) != 0) {
            __android_log_print(ANDROID_LOG_ERROR, TAG, "llama_decode failed for prompt");
            llama_batch_free(prompt_batch);
            return env->NewStringUTF("Error: llama_decode failed for prompt.");
        }
        llama_batch_free(prompt_batch);
    }

    std::string generated_text = "";
    const int max_new_tokens = 128; // Max tokens to generate
    int n_generated = 0;
    int n_cur = n_prompt_tokens; // Current position in the sequence for generation

    __android_log_print(ANDROID_LOG_INFO, TAG, "Generating up to %d tokens...", max_new_tokens);

    while (n_generated < max_new_tokens && n_cur < llama_n_ctx(g_ctx)) {
        float *logits = llama_get_logits_ith(g_ctx, -1); // Get logits for the last token processed
        if (!logits) {
            __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to get logits.");
            break;
        }

        llama_token new_token_id = 0;
        float max_p = -std::numeric_limits<float>::infinity();
        int current_vocab_size = llama_vocab_n_tokens(vocab);

        for (llama_token token_id = 0; token_id < current_vocab_size; ++token_id) {
            if (logits[token_id] > max_p) {
                max_p = logits[token_id];
                new_token_id = token_id;
            }
        }

        if (new_token_id == llama_vocab_eos(vocab)) {
            __android_log_print(ANDROID_LOG_INFO, TAG, "EOS token encountered.");
            break;
        }

        char token_text_buffer[32]; // Buffer for token text
        int n_chars = llama_token_to_piece(vocab, new_token_id, token_text_buffer, sizeof(token_text_buffer), 0, true);
        if (n_chars > 0) {
            generated_text += std::string(token_text_buffer, n_chars);
        } else if (n_chars < 0) {
             __android_log_print(ANDROID_LOG_WARN, TAG, "llama_token_to_piece failed with error %d for token %d", n_chars, new_token_id);
        } else { // n_chars == 0
             __android_log_print(ANDROID_LOG_WARN, TAG, "llama_token_to_piece returned empty for token %d", new_token_id);
        }

        llama_batch gen_batch = llama_batch_init(1, 0, 1); // Batch for one token
        if (!gen_batch.token) {
            __android_log_print(ANDROID_LOG_ERROR, TAG, "llama_batch_init failed for generation batch.");
            break; 
        }
        gen_batch.token[0] = new_token_id;
        gen_batch.pos[0] = n_cur;
        gen_batch.n_seq_id[0] = 1;
        if (gen_batch.seq_id && gen_batch.seq_id[0]) {
            gen_batch.seq_id[0][0] = 0; // Sequence ID 0
        } else {
             __android_log_print(ANDROID_LOG_ERROR, TAG, "Generation batch seq_id structure not allocated as expected.");
            llama_batch_free(gen_batch);
            break;
        }
        gen_batch.logits[0] = true; // Request logits for this token
        gen_batch.n_tokens = 1;

        if (llama_decode(g_ctx, gen_batch) != 0) {
            __android_log_print(ANDROID_LOG_ERROR, TAG, "llama_decode failed for next token");
            llama_batch_free(gen_batch);
            break;
        }
        llama_batch_free(gen_batch);

        n_cur++;
        n_generated++;
    }

    __android_log_print(ANDROID_LOG_INFO, TAG, "Inference finished. Generated: %s", generated_text.c_str());
    return env->NewStringUTF(generated_text.c_str());
}
