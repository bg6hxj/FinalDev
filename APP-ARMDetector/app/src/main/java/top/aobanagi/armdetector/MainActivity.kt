package top.aobanagi.armdetector

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Dashboard
import androidx.compose.material.icons.filled.History
import androidx.compose.material.icons.filled.Notifications
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.ShowChart
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.navigation.NavDestination.Companion.hierarchy
import androidx.navigation.NavGraph.Companion.findStartDestination
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.compose.rememberNavController
import top.aobanagi.armdetector.ui.screens.alerts.AlertsScreen
import top.aobanagi.armdetector.ui.screens.control.DeviceControlScreen
import top.aobanagi.armdetector.ui.screens.dashboard.DashboardScreen
import top.aobanagi.armdetector.ui.screens.history.HistoryDataScreen
import top.aobanagi.armdetector.ui.screens.realtime.RealtimeDataScreen
import top.aobanagi.armdetector.ui.theme.ARMDetectorTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            ARMDetectorTheme {
                MainScreen()
            }
        }
    }
}

@Composable
fun MainScreen() {
    val navController = rememberNavController()

    val items = listOf(
        Screen.Dashboard,
        Screen.Alerts,
        Screen.Realtime,
        Screen.History,
        Screen.Control
    )

    Scaffold(
        bottomBar = {
            NavigationBar {
                val navBackStackEntry by navController.currentBackStackEntryAsState()
                val currentDestination = navBackStackEntry?.destination

                items.forEach { screen ->
                    NavigationBarItem(
                        icon = { Icon(screen.icon, contentDescription = screen.label) },
                        label = { Text(screen.label) },
                        selected = currentDestination?.hierarchy?.any { it.route == screen.route } == true,
                        onClick = {
                            navController.navigate(screen.route) {
                                popUpTo(navController.graph.findStartDestination().id) {
                                    saveState = true
                                }
                                launchSingleTop = true
                                restoreState = true
                            }
                        }
                    )
                }
            }
        }
    ) { innerPadding ->
        NavHost(
            navController = navController,
            startDestination = Screen.Dashboard.route,
            modifier = Modifier.padding(innerPadding)
        ) {
            composable(Screen.Dashboard.route) { DashboardScreen() }
            composable(Screen.Alerts.route) { AlertsScreen() }
            composable(Screen.Realtime.route) { RealtimeDataScreen() }
            composable(Screen.History.route) { HistoryDataScreen() }
            composable(Screen.Control.route) { DeviceControlScreen() }
        }
    }
}

sealed class Screen(val route: String, val label: String, val icon: androidx.compose.ui.graphics.vector.ImageVector) {
    object Dashboard : Screen("dashboard", "总览", Icons.Default.Dashboard)
    object Alerts : Screen("alerts", "警报", Icons.Default.Notifications)
    object Realtime : Screen("realtime", "实时", Icons.Default.ShowChart)
    object History : Screen("history", "历史", Icons.Default.History)
    object Control : Screen("control", "控制", Icons.Default.Settings)
}