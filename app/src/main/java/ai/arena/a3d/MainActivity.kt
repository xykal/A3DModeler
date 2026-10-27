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
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
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
                Modifier.fillMaxWidth().height(48.dp).background(Color(0xFF202329))
                    .horizontalScroll(rememberScrollState()).padding(horizontal = 8.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                TextButton(onClick = { EditorViewportRegistry.viewport?.selectionMode(0) }) { Text("Vertex") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.selectionMode(1) }) { Text("Edge") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.selectionMode(2) }) { Text("Face") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.toggleAdditiveSelection() }) { Text("Multi") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.activateBoxSelect() }) { Text("Box") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.selectAll() }) { Text("All") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.invertSelection() }) { Text("Invert") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.clearSelection() }) { Text("Clear") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.toggleSnapping() }) { Text("Snap") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.activateMoveTool(-1) }) { Text("Move") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.activateMoveTool(0) }) { Text("Move X") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.activateMoveTool(1) }) { Text("Move Y") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.activateMoveTool(2) }) { Text("Move Z") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.activateRotateTool(0) }) { Text("Rot X") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.activateRotateTool(1) }) { Text("Rot Y") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.activateRotateTool(2) }) { Text("Rot Z") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.activateScaleTool(-1) }) { Text("Scale") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.activateScaleTool(0) }) { Text("Scale X") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.activateScaleTool(1) }) { Text("Scale Y") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.activateScaleTool(2) }) { Text("Scale Z") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.mergeVertices() }) { Text("Merge") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.extrude(0.5f) }) { Text("Extrude") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.inset(0.2f) }) { Text("Inset") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.flipFace() }) { Text("Flip") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.deleteFace() }) { Text("Delete") }
                Spacer(Modifier.width(16.dp))
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
    private var moveToolActive = false
    private var transformTool = 0 // 0 move, 1 rotate, 2 scale
    private var moveAxis = -1
    private var snappingEnabled = false
    private var additiveSelection = false
    private var boxSelectActive = false
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
                if (moveToolActive) {
                    val x = event.x
                    val y = event.y
                    val axis = moveAxis
                    queueEvent { NativeRenderer.nativeBeginMeshTransform(x, y, axis) }
                }
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
                    if (moveToolActive) {
                        when (transformTool) {
                            1 -> queueEvent { NativeRenderer.nativeDragRotateTransform(dx, dy) }
                            2 -> queueEvent { NativeRenderer.nativeDragScaleTransform(dx, dy) }
                            else -> queueEvent { NativeRenderer.nativeDragMeshTransform(dx, dy) }
                        }
                    } else if (!boxSelectActive) {
                        queueEvent { NativeRenderer.nativeOrbit(dx, dy) }
                    }
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
            MotionEvent.ACTION_UP -> {
                if (moveToolActive) {
                    queueEvent { NativeRenderer.nativeEndMeshTransform() }
                } else if (boxSelectActive && moved) {
                    val x1 = downX; val y1 = downY; val x2 = event.x; val y2 = event.y
                    queueEvent { NativeRenderer.nativeBoxSelect(x1, y1, x2, y2) }
                } else if (!moved) {
                    val x = event.x
                    val y = event.y
                    queueEvent { NativeRenderer.nativePick(x, y) }
                }
            }
            MotionEvent.ACTION_CANCEL -> if (moveToolActive) {
                queueEvent { NativeRenderer.nativeCancelMeshTransform() }
            }
        }
        return true
    }

    fun selectionMode(mode: Int) {
        moveToolActive = false
        boxSelectActive = false
        moveAxis = -1
        queueEvent { NativeRenderer.nativeSetSelectionMode(mode) }
    }
    fun activateBoxSelect() {
        moveToolActive = false
        boxSelectActive = true
    }
    fun activateMoveTool(axis: Int) {
        moveToolActive = true
        boxSelectActive = false
        transformTool = 0
        moveAxis = axis
    }
    fun activateRotateTool(axis: Int) {
        moveToolActive = true
        boxSelectActive = false
        transformTool = 1
        moveAxis = axis
    }
    fun activateScaleTool(axis: Int) {
        moveToolActive = true
        boxSelectActive = false
        transformTool = 2
        moveAxis = axis
    }
    fun toggleSnapping() {
        snappingEnabled = !snappingEnabled
        val enabled = snappingEnabled
        queueEvent { NativeRenderer.nativeSetSnapping(enabled, 0.25f, 15f, 0.1f) }
    }
    fun toggleAdditiveSelection() {
        additiveSelection = !additiveSelection
        val enabled = additiveSelection
        queueEvent { NativeRenderer.nativeSetAdditiveSelection(enabled) }
    }
    fun selectAll() = queueEvent { NativeRenderer.nativeSelectAll() }
    fun invertSelection() = queueEvent { NativeRenderer.nativeInvertSelection() }
    fun clearSelection() = queueEvent { NativeRenderer.nativeClearSelection() }
    fun nudge(axis: Int, amount: Float) = queueEvent { NativeRenderer.nativeNudge(axis, amount) }
    fun extrude(distance: Float) = queueEvent { NativeRenderer.nativeExtrude(distance) }
    fun inset(amount: Float) = queueEvent { NativeRenderer.nativeInset(amount) }
    fun mergeVertices() = queueEvent { NativeRenderer.nativeMergeVertices() }
    fun deleteFace() = queueEvent { NativeRenderer.nativeDeleteFace() }
    fun flipFace() = queueEvent { NativeRenderer.nativeFlipFace() }
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
    external fun nativeBoxSelect(x1: Float, y1: Float, x2: Float, y2: Float): Boolean
    external fun nativeSetSelectionMode(mode: Int)
    external fun nativeSetAdditiveSelection(enabled: Boolean)
    external fun nativeSetSnapping(enabled: Boolean, moveStep: Float, angleDegrees: Float, scaleStep: Float)
    external fun nativeSelectAll()
    external fun nativeInvertSelection()
    external fun nativeClearSelection()
    external fun nativeBeginMeshTransform(x: Float, y: Float, preferredAxis: Int): Boolean
    external fun nativeTranslateSelection(x: Float, y: Float, z: Float): Boolean
    external fun nativeDragMeshTransform(dx: Float, dy: Float): Boolean
    external fun nativeDragRotateTransform(dx: Float, dy: Float): Boolean
    external fun nativeDragScaleTransform(dx: Float, dy: Float): Boolean
    external fun nativeEndMeshTransform(): Boolean
    external fun nativeCancelMeshTransform()
    external fun nativeNudge(axis: Int, amount: Float): Boolean
    external fun nativeExtrude(distance: Float): Boolean
    external fun nativeInset(amount: Float): Boolean
    external fun nativeMergeVertices(): Boolean
    external fun nativeDeleteFace(): Boolean
    external fun nativeFlipFace(): Boolean
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
