
package com.example.test_app

import android.content.res.AssetManager
import android.os.Bundle
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth // 추가
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.OutlinedTextField // 추가
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.example.test_app.ui.theme.TEST_APPTheme
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

// 모델 상태 정의
sealed class ModelStatus {
    object NotLoaded : ModelStatus()
    object Loading : ModelStatus()
    data class Loaded(val message: String) : ModelStatus()
    data class Error(val message: String) : ModelStatus()
}

class MainActivity : ComponentActivity() {

    // 네이티브 메소드 선언
    private external fun stringFromJNI(): String
    private external fun loadModelJNI(assetManager: AssetManager, modelFileName: String, cacheDirPath: String): Boolean
    private external fun unloadModelJNI()
    private external fun generateTextJNI(prompt: String): String // <<< 새로운 JNI 함수 선언

    // private val modelFileNameInAssets = "qwen2.5-3b-instruct-q4_k_m.gguf" // 3b 테스트
    private val modelFileNameInAssets = "qwen2.5-1.5b-instruct-q4_k_m.gguf" // 1.5b 테스트


    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        setContent {
            TEST_APPTheme {
                var modelStatus by remember { mutableStateOf<ModelStatus>(ModelStatus.NotLoaded) }
                val coroutineScope = rememberCoroutineScope()
                val context = LocalContext.current

                // 추론 UI 상태 변수
                var prompt by remember { mutableStateOf("") }
                var generatedText by remember { mutableStateOf("") }
                var isGenerating by remember { mutableStateOf(false) }

                Scaffold(modifier = Modifier.fillMaxSize()) { innerPadding ->
                    Column(
                        modifier = Modifier
                            .fillMaxSize()
                            .padding(innerPadding)
                            .padding(16.dp),
                        horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.Center
                    ) {
                        when (val status = modelStatus) {
                            is ModelStatus.NotLoaded -> {
                                Text("Model is not loaded.")
                                Spacer(modifier = Modifier.height(16.dp))
                                Button(onClick = {
                                    modelStatus = ModelStatus.Loading
                                    coroutineScope.launch {
                                        Log.d("MainActivity", "Attempting to load model in background...")
                                        val success = withContext(Dispatchers.IO) { // IO 스레드에서 JNI 호출
                                            loadModelJNI(
                                                context.assets,
                                                modelFileNameInAssets,
                                                context.cacheDir.absolutePath
                                            )
                                        }
                                        if (success) {
                                            Log.d("MainActivity", "Model loaded successfully via JNI.")
                                            modelStatus = ModelStatus.Loaded(stringFromJNI()) // C++에서 오는 메시지 사용
                                        } else {
                                            Log.e("MainActivity", "Failed to load model via JNI.")
                                            modelStatus = ModelStatus.Error("Failed to load model. Check Logcat for C++ errors.")
                                        }
                                    }
                                }) {
                                    Text("Load Model")
                                }
                            }
                            is ModelStatus.Loading -> {
                                Text("Model is loading...")
                                Spacer(modifier = Modifier.height(16.dp))
                                CircularProgressIndicator()
                            }
                            is ModelStatus.Loaded -> {
                                Text(status.message) // stringFromJNI() 결과 표시
                                Spacer(modifier = Modifier.height(16.dp))

                                // --- 추론 UI 시작 ---
                                OutlinedTextField(
                                    value = prompt,
                                    onValueChange = { prompt = it },
                                    label = { Text("Enter prompt") },
                                    modifier = Modifier.fillMaxWidth()
                                )
                                Spacer(modifier = Modifier.height(8.dp))
                                Button(
                                    onClick = {
                                        if (prompt.isNotBlank()) {
                                            isGenerating = true
                                            generatedText = "" // 이전 결과 초기화
                                            coroutineScope.launch {
                                                Log.d("MainActivity", "Generating text for prompt: $prompt")
                                                val result = withContext(Dispatchers.IO) {
                                                    generateTextJNI(prompt) // JNI 호출
                                                }
                                                generatedText = result
                                                isGenerating = false
                                                Log.d("MainActivity", "Generated text: $result")
                                            }
                                        }
                                    },
                                    enabled = !isGenerating && prompt.isNotBlank() // 생성 중이 아닐 때 & 프롬프트가 있을 때 활성화
                                ) {
                                    Text("Generate")
                                }
                                Spacer(modifier = Modifier.height(16.dp))
                                if (isGenerating) {
                                    CircularProgressIndicator()
                                } else if (generatedText.isNotBlank()) {
                                    Text("Generated Text:", style = androidx.compose.material3.MaterialTheme.typography.titleMedium)
                                    Text(generatedText)
                                }
                                // --- 추론 UI 끝 ---
                            }
                            is ModelStatus.Error -> {
                                Text(status.message, color = androidx.compose.ui.graphics.Color.Red)
                                Spacer(modifier = Modifier.height(16.dp))
                                Button(onClick = { // 재시도 버튼 또는 초기화 버튼
                                    modelStatus = ModelStatus.NotLoaded
                                    // 추론 관련 상태도 초기화
                                    prompt = ""
                                    generatedText = ""
                                    isGenerating = false
                                }) {
                                    Text("Try Again / Reset")
                                }
                            }
                        }

                        Spacer(modifier = Modifier.height(32.dp))
                        // 기존 Greeting 함수는 stringFromJNI의 현재 상태를 간단히 보여주는 용도로 활용 가능
                        // 또는 모델 로드 성공 후 다른 정보 표시용으로 변경 가능
                        Greeting(name = "JNI Check: ${stringFromJNI()}", modifier = Modifier)
                    }
                }
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        Log.d("MainActivity", "onDestroy called, unloading model.")
        unloadModelJNI() // 앱 종료 시 모델 리소스 해제
    }

    companion object {
        init {
            System.loadLibrary("testapp-native")
        }
    }
}

@Composable
fun Greeting(name: String, modifier: Modifier = Modifier) {
    Text(
        text = name,
        modifier = modifier
    )
}

@Preview(showBackground = true)
@Composable
fun GreetingPreview() {
    TEST_APPTheme {
        Column(
            modifier = Modifier.fillMaxSize().padding(16.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.Center
        ) {
            // 모델 로드 전 UI 예시
            Text("Model is not loaded.")
            Spacer(modifier = Modifier.height(16.dp))
            Button(onClick = {}) {
                Text("Load Model")
            }
            Spacer(modifier = Modifier.height(32.dp))

            // 모델 로드 후 UI 예시 (추론 UI 포함)
            Text("Llama.cpp model loaded. Ready for inference!")
            Spacer(modifier = Modifier.height(16.dp))
            OutlinedTextField(
                value = "Who are you?",
                onValueChange = {},
                label = { Text("Enter prompt") },
                modifier = Modifier.fillMaxWidth()
            )
            Spacer(modifier = Modifier.height(8.dp))
            Button(onClick = {}) {
                Text("Generate")
            }
            Spacer(modifier = Modifier.height(16.dp))
            Text("Generated Text:", style = androidx.compose.material3.MaterialTheme.typography.titleMedium)
            Text("I am a large language model.")
            Spacer(modifier = Modifier.height(32.dp))

            Greeting("Preview: JNI not active")
        }
    }
}
