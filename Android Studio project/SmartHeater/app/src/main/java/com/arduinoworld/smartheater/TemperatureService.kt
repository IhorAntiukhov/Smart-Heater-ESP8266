package com.arduinoworld.smartheater

import android.app.*
import android.content.ContentResolver
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.IBinder
import android.widget.Toast
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import com.arduinoworld.smartheater.MainActivity.Companion.editPreferences
import com.arduinoworld.smartheater.MainActivity.Companion.firebaseAuth
import com.arduinoworld.smartheater.MainActivity.Companion.isUserLogged
import com.arduinoworld.smartheater.MainActivity.Companion.realtimeDatabase
import com.arduinoworld.smartheater.MainActivity.Companion.sharedPreferences
import com.google.firebase.database.DataSnapshot
import com.google.firebase.database.DatabaseError
import com.google.firebase.database.ValueEventListener

class TemperatureService : Service() {
    private var valueEventListenerStartedNow = false

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            realtimeDatabase.child(firebaseAuth.currentUser!!.uid).child("temperature")
                .addValueEventListener(temperatureNotificationListener)

            if (sharedPreferences.getBoolean("EnableHeaterStateNotifications", true)) {
                realtimeDatabase.child(firebaseAuth.currentUser!!.uid).child("isHeaterStarted")
                    .addValueEventListener(heaterStateListener)
            }
        }
        return START_STICKY
    }

    private val temperatureNotificationListener = object: ValueEventListener {
        override fun onDataChange(snapshot: DataSnapshot) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                val snapshotValue = snapshot.getValue(Int::class.java)!!
                val activityMainIntent = Intent(applicationContext, MainActivity::class.java)
                val pendingIntent = PendingIntent.getActivity(
                    applicationContext, 9876, activityMainIntent, PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
                editPreferences.putInt("LastTemperature", snapshotValue).apply()

                startForeground(9876, NotificationCompat.Builder(applicationContext, "temperatureNotification")
                    .setOngoing(true)
                    .setSmallIcon(R.drawable.ic_notification)
                    .setContentTitle(getString(R.string.temperature_notification_title))
                    .setContentText(getString(R.string.temperature_notification_text, snapshotValue))
                    .setCategory(NotificationCompat.CATEGORY_SERVICE)
                    .setPriority(NotificationCompat.PRIORITY_HIGH)
                    .setContentIntent(pendingIntent).build()
                )
            }
        }

        override fun onCancelled(error: DatabaseError) {
            if (isUserLogged) Toast.makeText(baseContext,"Не удалось получить температуру!", Toast.LENGTH_LONG).show()
        }

    }

    private val heaterStateListener = object: ValueEventListener {
        override fun onDataChange(snapshot: DataSnapshot) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                if (sharedPreferences.getInt("Temperature", -1) != -1) {
                    if (sharedPreferences.getBoolean("EnableHeaterStateNotifications", true)) {
                        if (valueEventListenerStartedNow) {
                            val snapshotValue = snapshot.getValue(Boolean::class.java)!!
                            val activityMainIntent = Intent(applicationContext, MainActivity::class.java)
                            val pendingIntent = PendingIntent.getActivity(applicationContext, 8765, activityMainIntent,
                                PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
                            )

                            val contentText = if (snapshotValue) {
                                getString(R.string.heater_started)
                            } else {
                                getString(R.string.heater_stopped)
                            }
                            val builder = NotificationCompat.Builder(applicationContext, "heaterOnOffNotification")
                                .setOngoing(false)
                                .setSmallIcon(R.drawable.ic_notification)
                                .setContentTitle(getString(R.string.temperature_notification_title))
                                .setContentText(contentText)
                                .setCategory(NotificationCompat.CATEGORY_SERVICE)
                                .setPriority(NotificationCompat.PRIORITY_HIGH)
                                .setContentIntent(pendingIntent)
                                .setSound(Uri.parse(ContentResolver.SCHEME_ANDROID_RESOURCE + "://" + baseContext.packageName + "/" + R.raw.notification))
                            with(NotificationManagerCompat.from(applicationContext)) {
                                notify(8765, builder.build())
                            }
                        } else {
                            valueEventListenerStartedNow = true
                        }
                    }
                }
            }
        }

        override fun onCancelled(error: DatabaseError) {
            if (isUserLogged) Toast.makeText(baseContext, "Не удалось получить\nто, запущен ли\nобогреватель!", Toast.LENGTH_LONG).show()
        }
    }

    override fun onBind(intent: Intent): IBinder {
        TODO("Return the communication channel to the service.")
    }

    override fun onDestroy() {
        super.onDestroy()
        realtimeDatabase.child(firebaseAuth.currentUser!!.uid).child("temperature")
            .removeEventListener(temperatureNotificationListener)
        realtimeDatabase.child(firebaseAuth.currentUser!!.uid).child("isHeaterStarted")
            .removeEventListener(heaterStateListener)
    }
}