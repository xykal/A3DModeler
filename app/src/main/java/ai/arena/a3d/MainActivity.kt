package ai.arena.a3d

import android.app.ActivityManager
import android.content.Context
import android.opengl.GLSurfaceView
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import ai.arena.a3d.ui.theme.A3DTheme
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent { A3DTheme { EditorScreen() } }
    }
}

@Composable
private fun EditorScreen() {
    var mode by remember { mutableStateOf("Object") }
    Scaffold(
        topBar = {
            Surface(color = Color(0xFF202329), tonalElevation = 4.dp) {
                Row(
                    Modifier.fillMaxWidth().height(52.dp).padding(horizontal = 12.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text("A3D", style = MaterialTheme.typography.titleLarge, color = Color(0xFF9BB0FF))
                    Spacer(Modifier.width(16.dp))
                    TextButton(onClick = { mode = if (mode == "Object") "Edit" else "Object" }) {
                        Text("$mode Mode")
                    }
                    Spacer(Modifier.weight(1f))
                    Text("Untitled", color = Color(0xFFBEC2CC))
                }
            }
        },
        bottomBar = {
            Row(
                Modifier.fillMaxWidth().height(44.dp).background(Color(0xFF202329)).padding(horizontal = 8.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                listOf("Select", "Move", "Rotate", "Scale").forEach { tool ->
                    TextButton(onClick = {}) { Text(tool) }
                }
                Spacer(Modifier.weight(1f))
                Text("OpenGL ES • Dual ABI", color = Color(0xFF8E939E), style = MaterialTheme.typography.labelSmall)
            }
        }
    ) { padding ->
        NativeViewport(Modifier.fillMaxSize().padding(padding))
    }
}

@Composable
private fun NativeViewport(modifier: Modifier = Modifier) {
    AndroidView(
        modifier = modifier,
        factory = { context ->
            checkOpenGles31(context)
            GLSurfaceView(context).apply {
                setEGLContextClientVersion(3)
                setEGLConfigChooser(8, 8, 8, 8, 24, 8)
                setRenderer(NativeRenderer)
                renderMode = GLSurfaceView.RENDERMODE_CONTINUOUSLY
                preserveEGLContextOnPause = true
            }
        }
    )
}

private fun checkOpenGles31(context: Context) {
    val manager = context.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
    require(manager.deviceConfigurationInfo.reqGlEsVersion >= 0x30001) {
        "OpenGL ES 3.1 is required"
    }
}

private object NativeRenderer : GLSurfaceView.Renderer {
    init { System.loadLibrary("a3d_core") }
    external fun nativeSurfaceCreated()
    external fun nativeSurfaceChanged(width: Int, height: Int)
    external fun nativeDrawFrame(timeSeconds: Float)
    private val startedAt = System.nanoTime()

    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) = nativeSurfaceCreated()
    override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) = nativeSurfaceChanged(width, height)
    override fun onDrawFrame(gl: GL10?) {
        nativeDrawFrame((System.nanoTime() - startedAt) / 1_000_000_000f)
    }
}
