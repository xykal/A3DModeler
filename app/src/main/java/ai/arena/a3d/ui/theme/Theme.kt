package ai.arena.a3d.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

private val Colors = darkColorScheme(
    primary = Color(0xFF9BB0FF),
    secondary = Color(0xFF75D5C6),
    background = Color(0xFF15171C),
    surface = Color(0xFF202329)
)

@Composable
fun A3DTheme(content: @Composable () -> Unit) {
    MaterialTheme(colorScheme = Colors, content = content)
}
