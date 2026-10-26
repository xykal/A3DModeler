# Spesifikasi Produk — Android 3D Modeler

**Status:** Draft v0.1  
**Target:** Android phone dan tablet  
**Teknologi utama:** Native Android (Kotlin) + C++/NDK  
**Fokus awal:** Editor 3D umum untuk membuat aset game

## 1. Visi

Membuat aplikasi modeling 3D native Android yang nyaman dipakai dengan sentuhan maupun stylus. Produk diarahkan menjadi alternatif mobile untuk workflow pembuatan aset game, bukan menyalin seluruh Blender sekaligus.

Prinsip produk:

- Lancar pada perangkat mobile kelas menengah.
- UI responsif untuk HP dan tablet.
- Operasi modeling non-destruktif bila memungkinkan.
- Format terbuka dan mudah dipindahkan ke game engine atau desktop DCC.
- Penyimpanan lokal lebih dahulu; cloud dapat ditambahkan kemudian.
- Fitur kompleks dibangun bertahap di atas fondasi scene, mesh, command, dan renderer yang stabil.

## 2. Sasaran MVP

Pengguna dapat:

1. Membuat dan mengelola scene.
2. Menambah primitive mesh.
3. Memilih object dan mengubah transform.
4. Masuk ke edit mode untuk memilih vertex, edge, dan face.
5. Menjalankan operasi mesh dasar.
6. Mengatur material PBR sederhana.
7. Mengatur kamera dan lampu.
8. Menyimpan project lokal.
9. Mengimpor serta mengekspor aset dalam format umum.
10. Membatalkan dan mengulangi operasi secara aman.

## 3. Batas MVP

### Termasuk

- Scene hierarchy/outliner.
- Perspective dan orthographic viewport.
- Primitive: cube, plane, sphere, cylinder, cone.
- Object mode dan Edit mode.
- Selection: object, vertex, edge, face; box select.
- Transform gizmo: move, rotate, scale.
- Snap dasar: grid, vertex, edge.
- Mesh tools: extrude, inset, bevel dasar, loop cut, merge, delete, duplicate, recalculate normals.
- Material PBR dasar: base color, metallic, roughness, normal texture.
- Directional, point, dan spot light sederhana.
- Camera object.
- Import: OBJ dan glTF/GLB.
- Export: OBJ dan glTF/GLB.
- Autosave, recent projects, crash recovery dasar.
- Undo/redo berbasis command.

### Tidak termasuk pada MVP

- Sculpting tingkat lanjut.
- Rigging dan skinning.
- Timeline dan character animation.
- Geometry nodes.
- Simulation physics, cloth, fluid, dan particles.
- Render path-tracing.
- Texture painting lengkap.
- Plugin marketplace.
- Kolaborasi realtime.

Fitur-fitur tersebut masuk roadmap setelah fondasi MVP stabil.

## 4. Target Perangkat

### Minimum awal

- Android 10 atau lebih baru.
- ABI yang didukung: **armeabi-v7a (32-bit)** dan **arm64-v8a (64-bit)**.
- Distribusi APK/AAB menyediakan native library C++ untuk kedua ABI tersebut.
- Perangkat armeabi-v7a menggunakan OpenGL ES 3.1 sebagai jalur kompatibilitas utama; Vulkan digunakan hanya setelah lolos pemeriksaan capability dan driver saat runtime.
- RAM minimum 3 GB untuk armeabi-v7a dan 4 GB untuk arm64-v8a.
- Penyimpanan bebas minimal 1 GB.
- x86/x86_64 tidak menjadi target perangkat produksi; x86_64 boleh disediakan hanya untuk emulator dan pengujian.

### Batas kompatibilitas armeabi-v7a

- Proses 32-bit memiliki ruang alamat yang lebih sempit sehingga ukuran scene, texture, undo history, dan cache harus dibatasi lebih ketat.
- Target praktis awal: sekitar 50 ribu–150 ribu triangle, menyesuaikan GPU dan kompleksitas material.
- Texture default dibatasi maksimal 2K dan diturunkan otomatis bila memory budget tidak cukup.
- Renderer memakai preset `Compatibility`, shadow lebih ringan, MSAA rendah/nonaktif, serta jumlah lampu realtime terbatas.
- Import dilakukan bertahap dengan pemeriksaan memory budget agar file besar tidak menyebabkan out-of-memory.
- Semua dependency native wajib dapat dikompilasi untuk 32-bit ARM dan tidak boleh mengasumsikan pointer 64-bit.
- CI wajib menghasilkan serta menguji library `armeabi-v7a` dan `arm64-v8a` pada setiap release.

### Rekomendasi

- Android 13+.
- RAM 8 GB atau lebih.
- GPU dengan Vulkan 1.2.
- Tablet dengan stylus pressure support untuk roadmap sculpting.

### Target performa MVP

- 60 FPS untuk scene ringan pada perangkat rekomendasi.
- Minimal 30 FPS saat berinteraksi dengan scene target MVP.
- Target praktis awal: 100 ribu–500 ribu triangle, bergantung GPU dan material.
- Waktu autosave tidak boleh membekukan UI secara terasa.

## 5. Arsitektur Teknis

### 5.1 Lapisan aplikasi

1. **Presentation — Kotlin + Jetpack Compose**
   - Workspace, toolbar, panels, dialogs, settings.
   - Adaptive layout untuk HP dan tablet.
   - ViewModel dan state UI.

2. **Editor Domain — C++**
   - Scene graph.
   - Mesh topology dan mesh operations.
   - Selection.
   - Transform.
   - Command stack dan undo/redo.
   - Serialization domain.

3. **Rendering — C++**
   - Backend Vulkan sebagai sasaran utama.
   - OpenGL ES sebagai fallback jika biaya implementasinya masih layak.
   - Render graph sederhana.
   - PBR raster renderer.
   - ID/picking pass untuk selection.
   - Grid, outline, gizmo, normals, wireframe overlay.

4. **Platform Bridge — JNI**
   - Surface lifecycle.
   - Input event batching.
   - File access melalui Android Storage Access Framework.
   - App lifecycle dan memory-pressure event.

5. **Import/Export**
   - glTF/GLB sebagai format interchange utama.
   - OBJ untuk kompatibilitas sederhana.
   - Library eksternal harus memiliki lisensi kompatibel.

### 5.2 Rekomendasi library

Evaluasi sebelum dikunci:

- **glm** untuk matematika.
- **cgltf** atau **tinygltf** untuk glTF.
- **tinyobjloader** untuk OBJ.
- **meshoptimizer** untuk optimasi mesh.
- **stb_image** atau decoder Android untuk tekstur.
- **Dear ImGui** hanya untuk debug tooling internal, bukan UI produksi utama.

Setiap dependency wajib dicatat versi, lisensi, ukuran binary, dan risiko maintenance.

### 5.3 Struktur repository

```text
/app                    Android application, Compose UI
/editor-core            C++ scene, mesh, commands, selection
/renderer               Vulkan/OpenGL rendering backend
/io                     project serialization, import/export
/native-bridge          JNI boundary
/shared-test-assets     legal test meshes and textures
/docs                    architecture, UX, ADR, file format
/benchmarks              performance and memory benchmarks
```

Gunakan Gradle Kotlin DSL dan CMake. CI membangun APK debug, menjalankan unit test Kotlin/C++, lint, dan pemeriksaan format.

## 6. Model Domain

### Scene

- `Scene`
- `Entity/Object`
- `Transform`
- `MeshComponent`
- `MaterialComponent`
- `LightComponent`
- `CameraComponent`
- Parent-child hierarchy

### Mesh

Representasi awal disarankan berupa indexed mesh dengan topology layer yang mendukung vertex-edge-face editing. Jangan bergantung langsung pada GPU vertex buffer sebagai sumber data utama. GPU buffer adalah cache yang dibangun ulang secara inkremental dari mesh editor.

Data minimum:

- Stable element IDs.
- Positions.
- Edges/faces/topology adjacency.
- Normals dan tangents.
- UV sets.
- Material slot per face.
- Selection state terpisah dari data mesh tersimpan.

### Command system

Semua perubahan editor harus menjadi command:

- `execute()`
- `undo()`
- `redo()`
- opsi command merge untuk drag transform
- transaction untuk operasi multi-step

Command stack memiliki batas memori. Command besar dapat menyimpan delta atau snapshot terkompresi berdasarkan jenis operasi.

## 7. Format Project

Ekstensi kerja sementara: `.a3dproj`.

Container dapat berupa ZIP dengan struktur:

```text
manifest.json
scene.bin
meshes/
materials/
textures/
thumbnails/
```

Ketentuan:

- Format memiliki nomor versi.
- Save dilakukan atomik: tulis file sementara, validasi, lalu rename.
- Migrasi format disiapkan sejak versi pertama.
- File project bukan pengganti ekspor GLB.
- Thumbnail dan metadata dapat dibaca tanpa memuat seluruh scene.

## 8. UX dan Layout

### Tablet

- Top app bar: project, save, undo/redo, workspace.
- Kiri: tool shelf.
- Tengah: viewport.
- Kanan: outliner dan properties.
- Bawah: status, selection info, contextual controls.

### HP

- Viewport menjadi area utama.
- Panels sebagai bottom sheet/drawer.
- Contextual toolbar berdasarkan mode.
- Radial menu atau quick actions untuk operasi sering dipakai.
- UI dapat disembunyikan untuk memaksimalkan viewport.

### Gesture

Default awal, dapat dikustomisasi:

- Satu jari: select/tap atau orbit berdasarkan active tool.
- Dua jari drag: pan.
- Pinch: zoom.
- Dua jari twist: opsional untuk orbit/roll; harus bisa dimatikan.
- Long press: context menu.
- Stylus button: shortcut configurable.

Karena gesture satu jari rawan konflik, aplikasi perlu mode navigasi eksplisit dan onboarding singkat.

## 9. Rendering MVP

- Raster-based PBR metallic-roughness.
- Environment color/HDRI opsional setelah renderer dasar.
- Shadow sederhana dengan quality tiers.
- MSAA sesuai kemampuan perangkat.
- Wireframe overlay tanpa bergantung pada desktop-only polygon mode.
- Selection lewat offscreen integer/color ID buffer.
- Frustum culling.
- Incremental GPU buffer update.
- Shader cache dan pipeline cache.
- Quality preset: Battery, Balanced, Quality.

## 10. Keamanan dan Privasi

- Project lokal tidak diunggah tanpa tindakan eksplisit pengguna.
- Gunakan Storage Access Framework; hindari izin penyimpanan luas.
- File impor dianggap tidak tepercaya: validasi ukuran, index, recursion depth, dan path traversal.
- Terapkan batas memori saat decode tekstur dan impor mesh.
- Crash report dan analytics harus opt-in/terjelaskan.
- Token GitHub tidak boleh ditulis ke source code atau dikirim melalui chat. Gunakan koneksi GitHub/OAuth atau secret store pada CI.

## 11. Pengujian

### Unit test

- Math dan transforms.
- Topology invariants.
- Setiap mesh command dan undo/redo.
- Serialization/version migration.
- glTF/OBJ edge cases.

### Integration test

- Create → edit → save → reopen.
- Import → modify → export → validate.
- App background/foreground saat project terbuka.
- Surface recreation dan orientation change.
- Low-memory behavior.

### Golden/render test

- Primitive, normals, PBR materials, light, grid, outline, picking.
- Toleransi vendor GPU perlu ditentukan.

### Device matrix awal

- Perangkat minimum RAM 4 GB.
- Perangkat kelas menengah populer.
- Tablet stylus.
- Minimal satu GPU Qualcomm, Mali, dan bila tersedia PowerVR.

## 12. Roadmap

### Fase 0 — Fondasi dan spike (2–4 minggu)

- Repository, CI, coding standards, ADR.
- Compose + native surface + JNI.
- Render triangle/cube dan input kamera.
- Benchmark Vulkan pada perangkat target.
- Validasi pilihan fallback renderer.

**Exit:** APK stabil menampilkan scene sederhana dan menangani lifecycle Android.

### Fase 1 — Core editor (6–10 minggu)

- Scene graph, outliner, object selection.
- Transform gizmo.
- Camera controls.
- Project create/save/open.
- Command system dan undo/redo.

**Exit:** Pengguna dapat menyusun scene object dasar dan menyimpannya.

### Fase 2 — Mesh editing (8–14 minggu)

- Topology representation.
- Vertex/edge/face selection.
- Extrude, inset, bevel dasar, loop cut, merge/delete.
- Wireframe, snapping, normals.

**Exit:** Aset low-poly sederhana dapat dibuat dari awal.

### Fase 3 — Material dan asset pipeline (6–10 minggu)

- PBR material editor.
- Texture handling.
- Import/export OBJ dan GLB.
- Validation dan interoperability test.

**Exit:** Aset dapat dipindahkan ke Godot, Unity, Unreal, atau Blender.

### Fase 4 — Alpha hardening (4–8 minggu)

- Autosave/recovery.
- Memory/performance optimization.
- Adaptive phone/tablet UX.
- Onboarding, sample project, QA device matrix.

**Exit:** Alpha terbatas layak diuji pengguna.

Estimasi di atas bersifat kasar untuk tim kecil berpengalaman dan perlu diubah setelah technical spike. Developer tunggal kemungkinan membutuhkan waktu jauh lebih panjang.

## 13. Roadmap PascamVP

Urutan yang disarankan:

1. UV editor dan texture painting dasar.
2. Modifiers utama: mirror, subdivision, solidify, array.
3. Sculpt mode dan stylus pressure.
4. Rigging, skinning, animation timeline.
5. Baking normal/AO untuk workflow high-poly ke low-poly.
6. Plugin/scripting API yang disandbox.
7. Cloud backup dan kolaborasi opsional.

## 14. Risiko Utama

| Risiko | Dampak | Mitigasi |
|---|---|---|
| Fragmentasi GPU/driver Android | Crash atau artefak | Device lab, validation layers, fallback quality, crash telemetry |
| Topology editor terlalu kompleks | Jadwal meleset | Batasi operasi MVP, invariant tests, stable IDs |
| Kehabisan RAM | App dihentikan OS | Streaming, texture limits, memory budget, incremental buffers |
| UI HP terlalu padat | Sulit digunakan | Contextual tools, sheets, radial actions, usability tests |
| Scope menyerupai Blender penuh | Produk tidak selesai | Gate fitur berdasarkan milestone dan exit criteria |
| File rusak saat save | Kehilangan kerja | Atomic save, autosave rotation, recovery validation |
| Lisensi dependency/aset | Risiko distribusi | Dependency/license audit sejak awal |

## 15. Keputusan yang Perlu Dikunci Setelah Spike

- Vulkan-only atau Vulkan + OpenGL ES fallback.
- Minimum Android API final.
- Struktur topology internal.
- Library glTF dan image decoding.
- Binary serialization library versus format custom.
- Nama produk, package ID, dan lisensi source code.
- Model distribusi: gratis, premium, subscription, atau open source.

## 16. Definition of Done MVP

MVP dinyatakan selesai bila:

- Semua acceptance flow utama lulus pada device matrix.
- Tidak ada bug kehilangan data yang diketahui.
- Undo/redo konsisten untuk semua tool yang dirilis.
- Project dapat dibuka kembali setelah app/process restart.
- GLB hasil ekspor tervalidasi dan berhasil dibuka di minimal Blender serta dua game engine.
- Performa memenuhi target scene yang telah ditentukan.
- Seluruh dependency dan sample asset memiliki catatan lisensi.
- APK release dapat dibangun ulang oleh CI dari repository bersih.

## 17. Langkah Berikutnya

1. Tentukan nama produk dan package ID sementara.
2. Putuskan lisensi serta model distribusi.
3. Tentukan tiga perangkat benchmark nyata.
4. Buat technical spike Vulkan + Compose + JNI.
5. Catat hasil spike dalam ADR sebelum implementasi core editor.
6. Setelah repository GitHub tersedia, gunakan koneksi GitHub/OAuth; jangan membagikan personal access token di chat.
