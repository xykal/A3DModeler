package ai.arena.a3d

import android.app.ActivityManager
import android.content.Context
import android.opengl.GLSurfaceView
import android.os.Bundle
import android.util.Log
import android.view.MotionEvent
import android.view.ScaleGestureDetector
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
                    TextButton(onClick = { EditorViewportRegistry.viewport?.undo() }) { Text("Undo") }
                    TextButton(onClick = { EditorViewportRegistry.viewport?.redo() }) { Text("Redo") }
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
                TextButton(onClick = {}) { Text("Select") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.nudge(0, 0.25f) }) { Text("X+") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.nudge(1, 0.25f) }) { Text("Y+") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.nudge(2, 0.25f) }) { Text("Z+") }
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
            EditorGLSurfaceView(context).also { EditorViewportRegistry.viewport = it }
        }
    )
}

private object EditorViewportRegistry {
    var viewport: EditorGLSurfaceView? = null
}

private class EditorGLSurfaceView(context: Context) : GLSurfaceView(context) {
    private var lastX = 0f
    private var lastY = 0f
    private var lastCentroidX = 0f
    private var lastCentroidY = 0f
    private var downX = 0f
    private var downY = 0f
    private var moved = false
    private val scaleDetector = ScaleGestureDetector(context,
        object : ScaleGestureDetector.SimpleOnScaleGestureListener() {
            override fun onScale(detector: ScaleGestureDetector): Boolean {
                val scale = detector.scaleFactor
                queueEvent { NativeRenderer.nativeZoom(scale) }
                return true
            }
        })

    init {
        setEGLContextClientVersion(3)
        setEGLConfigChooser(8, 8, 8, 8, 24, 8)
        setRenderer(NativeRenderer)
        renderMode = RENDERMODE_CONTINUOUSLY
        preserveEGLContextOnPause = true
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        scaleDetector.onTouchEvent(event)
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                lastX = event.x
                lastY = event.y
                downX = event.x
                downY = event.y
                moved = false
            }
            MotionEvent.ACTION_POINTER_DOWN -> if (event.pointerCount >= 2) {
                lastCentroidX = (event.getX(0) + event.getX(1)) * 0.5f
                lastCentroidY = (event.getY(0) + event.getY(1)) * 0.5f
            }
            MotionEvent.ACTION_MOVE -> {
                if (kotlin.math.abs(event.x - downX) > 8f || kotlin.math.abs(event.y - downY) > 8f) moved = true
                if (event.pointerCount == 1) {
                    val dx = event.x - lastX
                    val dy = event.y - lastY
                    queueEvent { NativeRenderer.nativeOrbit(dx, dy) }
                    lastX = event.x
                    lastY = event.y
                } else if (event.pointerCount >= 2) {
                    val cx = (event.getX(0) + event.getX(1)) * 0.5f
                    val cy = (event.getY(0) + event.getY(1)) * 0.5f
                    val dx = cx - lastCentroidX
                    val dy = cy - lastCentroidY
                    queueEvent { NativeRenderer.nativePan(dx, dy) }
                    lastCentroidX = cx
                    lastCentroidY = cy
                }
            }
            MotionEvent.ACTION_UP -> if (!moved) {
                val x = event.x
                val y = event.y
                queueEvent { NativeRenderer.nativePick(x, y) }
            }
        }
        return true
    }

    fun nudge(axis: Int, amount: Float) = queueEvent { NativeRenderer.nativeNudge(axis, amount) }
    fun undo() = queueEvent { NativeRenderer.nativeUndo() }
    fun redo() = queueEvent { NativeRenderer.nativeRedo() }

    override fun onDetachedFromWindow() {
        if (EditorViewportRegistry.viewport === this) EditorViewportRegistry.viewport = null
        queueEvent { NativeRenderer.nativeRelease() }
        super.onDetachedFromWindow()
    }
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
    external fun nativeDrawFrame()
    external fun nativeOrbit(dx: Float, dy: Float)
    external fun nativeZoom(scale: Float)
    external fun nativePan(dx: Float, dy: Float)
    external fun nativePick(x: Float, y: Float): Boolean
    external fun nativeNudge(axis: Int, amount: Float): Boolean
    external fun nativeUndo(): Boolean
    external fun nativeRedo(): Boolean
    external fun nativeCapabilities(): String
    external fun nativeRelease()

    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
        nativeSurfaceCreated()
        Log.i("A3D", nativeCapabilities())
    }
    override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) = nativeSurfaceChanged(width, height)
    override fun onDrawFrame(gl: GL10?) = nativeDrawFrame()
}
