package com.arduinoworld.smartheater

import android.content.Context
import android.os.Build
import android.os.Bundle
import android.os.VibrationEffect
import android.view.HapticFeedbackConstants
import android.view.inputmethod.InputMethodManager
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.android.volley.Request
import com.android.volley.toolbox.StringRequest
import com.android.volley.toolbox.Volley
import com.arduinoworld.smartheater.MainActivity.Companion.editPreferences
import com.arduinoworld.smartheater.MainActivity.Companion.firebaseAuth
import com.arduinoworld.smartheater.MainActivity.Companion.isHapticFeedbackEnabled
import com.arduinoworld.smartheater.MainActivity.Companion.realtimeDatabase
import com.arduinoworld.smartheater.MainActivity.Companion.sharedPreferences
import com.arduinoworld.smartheater.MainActivity.Companion.vibrator
import com.arduinoworld.smartheater.databinding.ActivityWorkParametersBinding


class WorkParameters : AppCompatActivity() {
    private lateinit var binding: ActivityWorkParametersBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityWorkParametersBinding.inflate(layoutInflater)
        setContentView(binding.root)

        with(binding) {
            inputWiFiNetwork.setText(sharedPreferences.getString("WiFiNetwork", ""))
            inputWiFiPassword.setText(sharedPreferences.getString("WiFiPassword", ""))
            inputTimezone.setText(sharedPreferences.getString("Timezone", ""))
            inputTemperatureInterval.setText(sharedPreferences.getString("TemperatureInterval", ""))

            layoutFirstWave.translationY = -575f
            layoutSecondWave.translationY = -575f
            inputLayoutWiFiNetwork.translationX = 800f
            inputLayoutWiFiPassword.translationX = 800f
            inputLayoutTimezone.translationX = 800f
            inputLayoutTemperatureInterval.translationX = 800f
            buttonSendWorkParameters.translationX = 800f

            inputLayoutWiFiNetwork.alpha = 0f
            inputLayoutWiFiPassword.alpha = 0f
            inputLayoutTimezone.alpha = 0f
            inputLayoutTemperatureInterval.alpha = 0f
            buttonSendWorkParameters.alpha = 0f

            layoutSecondWave.animate().translationY(0f).setDuration(500).setStartDelay(0).start()
            layoutFirstWave.animate().translationY(0f).setDuration(500).setStartDelay(200).start()
            inputLayoutWiFiNetwork.animate().translationX(0f).alpha(1f).setDuration(500).setStartDelay(0).start()
            inputLayoutWiFiPassword.animate().translationX(0f).alpha(1f).setDuration(500).setStartDelay(100).start()
            inputLayoutTimezone.animate().translationX(0f).alpha(1f).setDuration(500).setStartDelay(200).start()
            inputLayoutTemperatureInterval.animate().translationX(0f).alpha(1f).setDuration(500).setStartDelay(300).start()
            buttonSendWorkParameters.animate().translationX(0f).alpha(1f).setDuration(500).setStartDelay(400).start()

            buttonBack.setOnClickListener {
                vibrate()
                onBackPressedDispatcher.onBackPressed()
                finish()
            }

            buttonSendWorkParameters.setOnClickListener {
                vibrate()
                if (inputWiFiNetwork.text!!.isNotEmpty() && inputWiFiPassword.text!!.isNotEmpty()
                    && inputTimezone.text!!.isNotEmpty() && inputWiFiPassword.text!!.length >= 8
                    && inputTimezone.text!!.toString().toInt() in -12..12
                    && inputTemperatureInterval.text!!.isNotEmpty()) {
                    hideKeyboard()
                    val alertDialogDeleteUserBuilder = androidx.appcompat.app.AlertDialog.Builder(this@WorkParameters)
                    alertDialogDeleteUserBuilder.setTitle("Отправка параметров")
                    alertDialogDeleteUserBuilder.setMessage("Выберите способ отправки параметров работы:")
                    alertDialogDeleteUserBuilder.setPositiveButton("WiFi") { _, _ ->
                        vibrate()
                        inputLayoutWiFiNetwork.isErrorEnabled = false
                        inputLayoutWiFiPassword.isErrorEnabled = false
                        inputLayoutTimezone.isErrorEnabled = false

                        val stringRequest = StringRequest(
                            Request.Method.POST,
                            "http://192.168.4.1/?network_name=${inputWiFiNetwork.text!!}&network_pass=${inputWiFiPassword.text!!}" +
                                    "&user_email=${sharedPreferences.getString("UserEmail", "")}&user_pass=${sharedPreferences.getString("UserPassword", "")}" +
                                    "&timezone=${inputTimezone.text!!}&interval=${inputTemperatureInterval.text!!}",
                            {
                                Toast.makeText(baseContext, "Параметры работы отправлены!", Toast.LENGTH_SHORT).show()
                            },
                            {
                                Toast.makeText(baseContext, "Не удалось отправить\nпараметры работы!", Toast.LENGTH_LONG).show()
                            }
                        )
                        Volley.newRequestQueue(this@WorkParameters).add(stringRequest)
                    }
                    alertDialogDeleteUserBuilder.setNegativeButton("Firebase") { _, _ ->
                        vibrate()
                        realtimeDatabase.child(firebaseAuth.currentUser!!.uid).child("workParameters")
                            .setValue("${inputWiFiNetwork.text!!}#${inputWiFiPassword.text!!}" +
                                    "#${sharedPreferences.getString("UserEmail", "")}#${sharedPreferences.getString("UserPassword", "")}" +
                                    "#${inputTimezone.text!!}#${inputTemperatureInterval.text!!}")
                            .addOnCompleteListener { setValueTask ->
                                if (setValueTask.isSuccessful) {
                                    Toast.makeText(baseContext, "Параметры работы отправлены!", Toast.LENGTH_SHORT).show()
                                } else {
                                    Toast.makeText(baseContext, "Не удалось отправить\nпараметры работы!", Toast.LENGTH_LONG).show()
                                }
                            }
                    }
                    val alertDialogDeleteUser = alertDialogDeleteUserBuilder.create()
                    alertDialogDeleteUser.show()

                    editPreferences.putString("WiFiNetwork", inputWiFiNetwork.text.toString())
                    editPreferences.putString("WiFiPassword", inputWiFiPassword.text.toString())
                    editPreferences.putString("Timezone", inputTimezone.text.toString())
                    editPreferences.putString("TemperatureInterval", inputTemperatureInterval.text.toString()).apply()
                } else {
                    if (inputWiFiNetwork.text!!.isEmpty()) {
                        inputLayoutWiFiNetwork.isErrorEnabled = true
                        inputLayoutWiFiNetwork.error = "Введите название WiFi сети"
                    } else {
                        inputLayoutWiFiNetwork.isErrorEnabled = false
                    }
                    if (inputWiFiPassword.text!!.isEmpty()) {
                        inputLayoutWiFiPassword.isErrorEnabled = true
                        inputLayoutWiFiPassword.error = "Введите пароль WiFi сети"
                    } else {
                        if (inputWiFiPassword.text!!.length < 8) {
                            inputLayoutWiFiPassword.isErrorEnabled = true
                            inputLayoutWiFiPassword.error = "Пароль должен быть не меньше 8 символов"
                        } else {
                            inputLayoutWiFiPassword.isErrorEnabled = false
                        }
                    }
                    if (inputTimezone.text!!.isEmpty()) {
                        inputLayoutTimezone.isErrorEnabled = true
                        inputLayoutTimezone.error = "Введите ваш часовой пояс"
                    } else {
                        if (inputTimezone.text!!.toString().toInt() <= -12 || inputTimezone.text!!.toString().toInt() >= 12) {
                            inputLayoutTimezone.isErrorEnabled = true
                            inputLayoutTimezone.error = "Часовой пояс должен быть в диапазоне от -12 до 12"
                        } else {
                            inputLayoutTimezone.isErrorEnabled = false
                        }
                    }
                    if (inputTemperatureInterval.text!!.isEmpty()) {
                        inputLayoutTemperatureInterval.isErrorEnabled = true
                        inputLayoutTemperatureInterval.error = "Введите интервал датчика"
                    } else {
                        inputLayoutTemperatureInterval.isErrorEnabled = false
                    }
                }
            }
        }
    }

    private fun hideKeyboard() {
        this.currentFocus?.let { view ->
            val inputMethodManager = getSystemService(Context.INPUT_METHOD_SERVICE) as? InputMethodManager
            inputMethodManager!!.hideSoftInputFromWindow(view.windowToken, 0)
        }
    }

    @Suppress("DEPRECATION")
    private fun vibrate() {
        if (vibrator.hasVibrator()) {
            if (isHapticFeedbackEnabled == "1") {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                    binding.buttonSendWorkParameters.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY, HapticFeedbackConstants.FLAG_IGNORE_VIEW_SETTING)
                } else {
                    binding.buttonSendWorkParameters.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY, HapticFeedbackConstants.FLAG_IGNORE_VIEW_SETTING + HapticFeedbackConstants.FLAG_IGNORE_GLOBAL_SETTING)
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