package com.arduinoworld.smartheater

import android.content.Intent
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle

class Splash : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val activity = Intent(this, MainActivity::class.java)
        startActivity(activity)
        finish()
    }
}