package com.arduinoworld.smartheater

import android.annotation.SuppressLint
import android.app.NotificationChannel
import android.app.NotificationChannelGroup
import android.app.NotificationManager
import android.content.ContentResolver
import android.content.Context
import android.content.Intent
import android.media.AudioAttributes
import android.net.Uri
import android.os.Build
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.os.VibrationEffect
import android.view.HapticFeedbackConstants
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.widget.Toast
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import com.arduinoworld.smartheater.MainActivity.Companion.editPreferences
import com.arduinoworld.smartheater.MainActivity.Companion.isHapticFeedbackEnabled
import com.arduinoworld.smartheater.MainActivity.Companion.isUserLogged
import com.arduinoworld.smartheater.MainActivity.Companion.sharedPreferences
import com.arduinoworld.smartheater.MainActivity.Companion.vibrator
import com.arduinoworld.smartheater.databinding.ActivitySettingsBinding

class SettingsActivity : AppCompatActivity() {
    companion object {
        @SuppressLint("StaticFieldLeak")
        private lateinit var binding: ActivitySettingsBinding
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivitySettingsBinding.inflate(layoutInflater)
        setContentView(binding.root)

        setSupportActionBar(binding.toolbarSettings)
        supportActionBar!!.title = getString(R.string.toolbar_settings)

        setTheme(R.style.SettingsStyle)

        if (savedInstanceState == null) {
            supportFragmentManager
                .beginTransaction()
                .replace(R.id.frameLayoutSettings, SettingsFragment())
                .commit()
        }
    }

    class SettingsFragment : PreferenceFragmentCompat() {
        
        override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
            setPreferencesFromResource(R.xml.preferences, rootKey)

            val showNotificationWithTemperature: Preference = preferenceManager.findPreference("ShowNotificationWithTemperature")!!
            val enableHeaterStateNotifications: Preference = preferenceManager.findPreference("EnableHeaterStateNotifications")!!
            val reducingTheMinTemperature: Preference = preferenceManager.findPreference("ReducingTheMinTemperature")!!
            val increasingTheMaxTemperature: Preference = preferenceManager.findPreference("IncreasingTheMaxTemperature")!!
            val isHapticFeedbackEnabled: Preference = preferenceManager.findPreference("isHapticFeedbackEnabled")!!
            val buttonDefaultSettings: Preference = preferenceManager.findPreference("buttonDefaultSettings")!!

            showNotificationWithTemperature.setOnPreferenceChangeListener { _, newValue ->
                vibrate()
                if (isUserLogged) {
                    if (newValue as Boolean) {
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                            if (!sharedPreferences.getBoolean("TemperatureNotificationChannelCreated", false)) {
                                val notificationManager = requireActivity().getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
                                notificationManager.createNotificationChannelGroup(NotificationChannelGroup("SmartHeater", "Умный Обогрев"))
                                val notificationChannel = NotificationChannel("temperatureNotification", "Уведомление с температурой", NotificationManager.IMPORTANCE_HIGH)
                                notificationChannel.description = "Уведомление с температурой, которое отображается постоянно"
                                notificationChannel.enableLights(false)
                                notificationChannel.enableVibration(false)
                                notificationChannel.setSound(null, null)
                                notificationChannel.group = "SmartHeater"
                                notificationChannel.lockscreenVisibility = View.VISIBLE
                                notificationManager.createNotificationChannel(notificationChannel)
                                editPreferences.putBoolean("TemperatureNotificationChannelCreated", true).apply()
                            }
                            requireActivity().startService(Intent(requireActivity(), TemperatureService::class.java))
                        } else {
                            Toast.makeText(requireActivity(), "Ваша версия Android\nменьше 8.0!", Toast.LENGTH_LONG).show()
                        }
                    } else {
                        requireActivity().stopService(Intent(requireActivity(), TemperatureService::class.java))
                    }
                }
                return@setOnPreferenceChangeListener true
            }
            enableHeaterStateNotifications.setOnPreferenceChangeListener { _, newValue ->
                vibrate()
                if (newValue as Boolean) {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                        if (!sharedPreferences.getBoolean("HeaterOnOffNotificationChannelCreated", false)) {
                            val notificationManager = requireActivity().getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
                            val notificationChannel = NotificationChannel("heaterOnOffNotification", "Уведомление с состоянием обогревателя", NotificationManager.IMPORTANCE_HIGH)
                            notificationChannel.description = "Уведомление, которое появляется при включении, или выключении обогревателя"
                            notificationChannel.enableLights(true)
                            notificationChannel.enableVibration(true)
                            val audioAttributes = AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_NOTIFICATION).build()
                            notificationChannel.setSound(Uri.parse(ContentResolver.SCHEME_ANDROID_RESOURCE + "://" + requireActivity().packageName + "/" + R.raw.notification), audioAttributes)
                            notificationChannel.group = "SmartHeater"
                            notificationChannel.lockscreenVisibility = View.VISIBLE
                            notificationManager.createNotificationChannel(notificationChannel)
                            editPreferences.putBoolean("HeaterOnOffNotificationChannelCreated", true).apply()
                        }
                    } else {
                        Toast.makeText(requireActivity(), "Ваша версия Android\nменьше 8.0!", Toast.LENGTH_LONG).show()
                    }
                }
                return@setOnPreferenceChangeListener true
            }

            reducingTheMinTemperature.setOnPreferenceClickListener {
                vibrate()
                return@setOnPreferenceClickListener true
            }
            reducingTheMinTemperature.setOnPreferenceChangeListener { _, _ ->
                vibrate()
                return@setOnPreferenceChangeListener true
            }

            increasingTheMaxTemperature.setOnPreferenceClickListener {
                vibrate()
                return@setOnPreferenceClickListener true
            }
            increasingTheMaxTemperature.setOnPreferenceChangeListener { _, _ ->
                vibrate()
                return@setOnPreferenceChangeListener true
            }

            isHapticFeedbackEnabled.setOnPreferenceClickListener {
                vibrate()
                return@setOnPreferenceClickListener true
            }
            isHapticFeedbackEnabled.setOnPreferenceChangeListener { _, newValue ->
                vibrate()
                MainActivity.isHapticFeedbackEnabled = newValue as String
                return@setOnPreferenceChangeListener true
            }

            buttonDefaultSettings.setOnPreferenceClickListener {
                vibrate()
                MainActivity.isHapticFeedbackEnabled = "1"
                editPreferences.putBoolean("ShowNotificationWithTemperature", false)
                editPreferences.putBoolean("EnableHeaterStateNotifications", true)
                editPreferences.putString("ReducingTheMinTemperature", "2")
                editPreferences.putString("IncreasingTheMaxTemperature", "1")
                editPreferences.putString("isHapticFeedbackEnabled", "1").apply()
                Toast.makeText(requireActivity(), "Настройки по умолчанию\nсохранены в память!", Toast.LENGTH_SHORT).show()
                return@setOnPreferenceClickListener true
            }
        }

        @Suppress("DEPRECATION")
        private fun vibrate() {
            if (vibrator.hasVibrator()) {
                if (isHapticFeedbackEnabled == "1") {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                        binding.buttonForHapticFeedback.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY, HapticFeedbackConstants.FLAG_IGNORE_VIEW_SETTING)
                    } else {
                        binding.buttonForHapticFeedback.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY, HapticFeedbackConstants.FLAG_IGNORE_VIEW_SETTING + HapticFeedbackConstants.FLAG_IGNORE_GLOBAL_SETTING)
                    }
                } else {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                        vibrator.vibrate(VibrationEffect.createOneShot(20, VibrationEffect.DEFAULT_AMPLITUDE))
                    } else {
                        vibrator.vibrate(20)
                    }
                }
            }
        }
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        val actionButtonsInflater = menuInflater
        actionButtonsInflater.inflate(R.menu.settings_activity_menu, menu)
        return super.onCreateOptionsMenu(menu)
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when(item.itemId) {
            R.id.buttonHome -> {
                vibrate()
                val activity = Intent(this, MainActivity::class.java)
                startActivity(activity)
                true
            }
            R.id.buttonUserProfile -> {
                vibrate()
                val activity = Intent(this, UserProfile::class.java)
                startActivity(activity)
                true
            }
            R.id.buttonWorkParameters -> {
                vibrate()
                val activity = Intent(this, WorkParameters::class.java)
                startActivity(activity)
                true
            }
            else->super.onOptionsItemSelected(item)
        }
    }

    @Suppress("DEPRECATION")
    private fun vibrate() {
        if (vibrator.hasVibrator()) {
            if (isHapticFeedbackEnabled == "1") {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                    binding.buttonForHapticFeedback.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY, HapticFeedbackConstants.FLAG_IGNORE_VIEW_SETTING)
                } else {
                    binding.buttonForHapticFeedback.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY, HapticFeedbackConstants.FLAG_IGNORE_VIEW_SETTING + HapticFeedbackConstants.FLAG_IGNORE_GLOBAL_SETTING)
                }
            } else {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    vibrator.vibrate(VibrationEffect.createOneShot(20, VibrationEffect.DEFAULT_AMPLITUDE))
                } else {
                    vibrator.vibrate(20)
                }
            }
        }
    }
}