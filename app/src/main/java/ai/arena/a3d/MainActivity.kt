package ai.arena.a3d

import android.app.ActivityManager
import android.content.Context
import android.opengl.GLSurfaceView
import androidx.activity.result.contract.ActivityResultContracts
import android.os.Bundle
import android.util.Log
import android.view.MotionEvent
import android.view.ScaleGestureDetector
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.clickable
import androidx.compose.foundation.border
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
    private val createProject = registerForActivityResult(
        ActivityResultContracts.CreateDocument("application/octet-stream")
    ) { uri ->
        if (uri != null) EditorViewportRegistry.viewport?.saveProject { bytes ->
            runCatching { contentResolver.openOutputStream(uri, "wt")?.use { it.write(bytes) } }
        }
    }
    private val openProject = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) runCatching {
            val bytes = contentResolver.openInputStream(uri)?.use { input ->
                val data = input.readBytes()
                require(data.size <= 256 * 1024 * 1024)
                data
            }
            if (bytes != null) EditorViewportRegistry.viewport?.loadProject(bytes)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            A3DTheme {
                EditorScreen(
                    onSave = { createProject.launch("Untitled.a3dproj") },
                    onOpen = { openProject.launch(arrayOf("application/octet-stream", "application/zip", "*/*")) }
                )
            }
        }
    }
}

private data class OutlinerItem(
    val id: Long, val name: String, val primitive: Int, val visible: Boolean,
    val locked: Boolean, val active: Boolean, val vertices: Int, val edges: Int,
    val faces: Int, val triangles: Int
)

private fun parseSceneSummary(summary: String): List<OutlinerItem> = summary.lineSequence().mapNotNull { line ->
    val p = line.split('|')
    if (p.size != 10) null else runCatching {
        OutlinerItem(p[0].toLong(), p[1], p[2].toInt(), p[3] == "1", p[4] == "1",
            p[5] == "1", p[6].toInt(), p[7].toInt(), p[8].toInt(), p[9].toInt())
    }.getOrNull()
}.toList()

@Composable
private fun EditorScreen(onSave: () -> Unit, onOpen: () -> Unit) {
    var mode by remember { mutableStateOf("Object") }
    var outlinerItems by remember { mutableStateOf(emptyList<OutlinerItem>()) }
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
                    TextButton(onClick = onOpen) { Text("Open") }
                    TextButton(onClick = onSave) { Text("Save") }
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
                TextButton(onClick = { EditorViewportRegistry.viewport?.addPrimitive(0) }) { Text("+Cube") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.addPrimitive(1) }) { Text("+Plane") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.addPrimitive(2) }) { Text("+Sphere") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.addPrimitive(3) }) { Text("+Cylinder") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.addPrimitive(4) }) { Text("+Cone") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.duplicateObject() }) { Text("Dup Obj") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.deleteObject() }) { Text("Del Obj") }
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
                TextButton(onClick = { EditorViewportRegistry.viewport?.fillFace() }) { Text("Fill") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.duplicateFaces() }) { Text("Duplicate") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.triangulateFaces() }) { Text("Triangulate") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.extrude(0.5f) }) { Text("Extrude") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.inset(0.2f) }) { Text("Inset") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.bevelFace() }) { Text("Bevel") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.flipFace() }) { Text("Flip") }
                TextButton(onClick = { EditorViewportRegistry.viewport?.deleteFace() }) { Text("Delete") }
                Spacer(Modifier.width(16.dp))
                Text("OpenGL ES • Dual ABI", color = Color(0xFF8E939E), style = MaterialTheme.typography.labelSmall)
            }
        }
    ) { padding ->
        BoxWithConstraints(Modifier.fillMaxSize().padding(padding)) {
            if (maxWidth >= 700.dp) {
                Row(Modifier.fillMaxSize()) {
                    NativeViewport(Modifier.weight(1f).fillMaxHeight()) { outlinerItems = parseSceneSummary(it) }
                    OutlinerPanel(outlinerItems, Modifier.width(260.dp).fillMaxHeight())
                }
            } else {
                Column(Modifier.fillMaxSize()) {
                    OutlinerStrip(outlinerItems, Modifier.fillMaxWidth().height(82.dp))
                    NativeViewport(Modifier.weight(1f).fillMaxWidth()) { outlinerItems = parseSceneSummary(it) }
                }
            }
        }
    }
}

@Composable
private fun OutlinerPanel(sceneItems: List<OutlinerItem>, modifier: Modifier = Modifier) {
    Surface(modifier, color = Color(0xFF1B1E24), tonalElevation = 3.dp) {
        Column(Modifier.padding(8.dp)) {
            Text("OUTLINER", style = MaterialTheme.typography.labelMedium, color = Color(0xFF8E939E))
            Spacer(Modifier.height(6.dp))
            LazyColumn { items(sceneItems, key = { it.id }) { OutlinerEntry(it, Modifier.fillMaxWidth()) } }
        }
    }
}

@Composable
private fun OutlinerStrip(sceneItems: List<OutlinerItem>, modifier: Modifier = Modifier) {
    Surface(modifier, color = Color(0xFF1B1E24)) {
        LazyRow(contentPadding = PaddingValues(horizontal = 6.dp), verticalAlignment = Alignment.CenterVertically) {
            items(sceneItems, key = { it.id }) { OutlinerEntry(it, Modifier.width(190.dp)) }
        }
    }
}

@Composable
private fun OutlinerEntry(item: OutlinerItem, modifier: Modifier = Modifier) {
    val borderColor = if (item.active) Color(0xFF7C9CFF) else Color.Transparent
    var editing by remember(item.id) { mutableStateOf(false) }
    var draftName by remember(item.id, item.name) { mutableStateOf(item.name) }
    Column(modifier.padding(3.dp).border(1.dp, borderColor, MaterialTheme.shapes.small)
        .clickable { EditorViewportRegistry.viewport?.selectObject(item.id) }.padding(6.dp)) {
        if (editing) {
            OutlinedTextField(draftName, { draftName = it }, singleLine = true,
                textStyle = MaterialTheme.typography.labelMedium,
                modifier = Modifier.fillMaxWidth().height(42.dp))
        } else {
            Text(item.name, color = if (item.active) Color(0xFFAFC0FF) else Color(0xFFD6D8DE),
                style = MaterialTheme.typography.labelLarge, maxLines = 1)
        }
        Text("V ${item.vertices}  E ${item.edges}  F ${item.faces}  T ${item.triangles}",
            color = Color(0xFF858B96), style = MaterialTheme.typography.labelSmall, maxLines = 1)
        Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            Text(if (item.visible) "Hide" else "Show", color = Color(0xFF8FB3FF),
                style = MaterialTheme.typography.labelSmall,
                modifier = Modifier.clickable { EditorViewportRegistry.viewport?.setObjectVisible(item.id, !item.visible) })
            Text(if (item.locked) "Unlock" else "Lock", color = Color(0xFF8FB3FF),
                style = MaterialTheme.typography.labelSmall,
                modifier = Modifier.clickable { EditorViewportRegistry.viewport?.setObjectLocked(item.id, !item.locked) })
            Text(if (editing) "Save" else "Rename", color = Color(0xFF8FB3FF),
                style = MaterialTheme.typography.labelSmall,
                modifier = Modifier.clickable {
                    if (editing) EditorViewportRegistry.viewport?.renameObject(item.id, draftName.trim())
                    editing = !editing
                })
        }
    }
}

@Composable
private fun NativeViewport(modifier: Modifier = Modifier, onSceneChanged: (String) -> Unit) {
    AndroidView(
        modifier = modifier,
        factory = { context ->
            checkOpenGles31(context)
            EditorGLSurfaceView(context).also {
                EditorViewportRegistry.viewport = it
                it.setSceneListener(onSceneChanged)
                it.postDelayed({ it.refreshScene() }, 500)
            }
        },
        update = { it.setSceneListener(onSceneChanged) }
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
    private var sceneListener: ((String) -> Unit)? = null
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

    fun setSceneListener(listener: (String) -> Unit) { sceneListener = listener }
    private fun emitSceneFromGlThread() {
        val summary = NativeRenderer.nativeSceneSummary()
        post { sceneListener?.invoke(summary) }
    }
    fun refreshScene() = queueEvent { emitSceneFromGlThread() }
    fun saveProject(callback: (ByteArray) -> Unit) = queueEvent {
        val data = NativeRenderer.nativeSaveProject()
        if (data != null) post { callback(data) }
    }
    fun loadProject(data: ByteArray) = queueEvent {
        if (NativeRenderer.nativeLoadProject(data)) emitSceneFromGlThread()
    }
    fun addPrimitive(type: Int) = queueEvent {
        NativeRenderer.nativeAddPrimitive(type); emitSceneFromGlThread()
    }
    fun selectObject(id: Long) = queueEvent {
        NativeRenderer.nativeSelectObject(id); emitSceneFromGlThread()
    }
    fun duplicateObject() = queueEvent {
        NativeRenderer.nativeDuplicateObject(); emitSceneFromGlThread()
    }
    fun deleteObject() = queueEvent {
        NativeRenderer.nativeDeleteObject(); emitSceneFromGlThread()
    }
    fun setObjectVisible(id: Long, visible: Boolean) = queueEvent {
        NativeRenderer.nativeSetObjectVisible(id, visible); emitSceneFromGlThread()
    }
    fun setObjectLocked(id: Long, locked: Boolean) = queueEvent {
        NativeRenderer.nativeSetObjectLocked(id, locked); emitSceneFromGlThread()
    }
    fun renameObject(id: Long, name: String) = queueEvent {
        NativeRenderer.nativeRenameObject(id, name); emitSceneFromGlThread()
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
    fun fillFace() = queueEvent { NativeRenderer.nativeFillFace() }
    fun duplicateFaces() = queueEvent { NativeRenderer.nativeDuplicateFaces(0.5f) }
    fun triangulateFaces() = queueEvent { NativeRenderer.nativeTriangulateFaces() }
    fun bevelFace() = queueEvent { NativeRenderer.nativeBevelFace(0.2f, 0.15f) }
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
    external fun nativeAddPrimitive(type: Int): Long
    external fun nativeSelectObject(id: Long): Boolean
    external fun nativeDuplicateObject(): Long
    external fun nativeDeleteObject(): Boolean
    external fun nativeSetObjectVisible(id: Long, visible: Boolean): Boolean
    external fun nativeSetObjectLocked(id: Long, locked: Boolean): Boolean
    external fun nativeRenameObject(id: Long, name: String): Boolean
    external fun nativeSaveProject(): ByteArray?
    external fun nativeLoadProject(data: ByteArray): Boolean
    external fun nativeSceneSummary(): String
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
    external fun nativeFillFace(): Boolean
    external fun nativeDuplicateFaces(offset: Float): Boolean
    external fun nativeTriangulateFaces(): Boolean
    external fun nativeBevelFace(amount: Float, depth: Float): Boolean
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
