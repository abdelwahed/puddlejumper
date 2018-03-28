package edu.washington.cs.puddlejumper;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.ToggleButton;

import java.io.FileOutputStream;

public class MainActivity extends Activity {

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    @Override
    public void onStop() {
        super.onStop();
        ToggleButton fmcwButton = findViewById(R.id.toggleButton);
        fmcwButton.setChecked(false);

        ToggleButton recordButton = findViewById(R.id.recordToggle);
        recordButton.setChecked(false);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        if(checkSelfPermission(Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{Manifest.permission.RECORD_AUDIO}, 0);
        }

        SpectrogramView specView = findViewById(R.id.spectrogramView);
        new Thread(specView).start();

        ToggleButton fmcwButton = findViewById(R.id.toggleButton);
        fmcwButton.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton compoundButton, boolean isChecked) {
                if(isChecked) {
                    startFMCW();
                } else {
                    stopFMCW();
                }
            }
        });

        ToggleButton recordButton = findViewById(R.id.recordToggle);
        recordButton.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton compoundButton, boolean isChecked) {
                if(isChecked) {
                    if(checkSelfPermission(Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
                        compoundButton.setChecked(false);
                    } else {
                        startCapture();
                    }
                } else {
                    stopCapture();
                }
            }
        });

        Button logButton = findViewById(R.id.logButton);

        logButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                EditText logTime = findViewById(R.id.logTime);
                try {
                    int timeout = Integer.parseInt(logTime.getText().toString());
                    if (timeout <= 0) { throw new NumberFormatException(); }

                    float [] result = recordFor(timeout);

                    FileOutputStream fileOutputStream = openFileOutput(
                            "log.txt", Context.MODE_PRIVATE
                    );

                    for(float v : result) {
                        fileOutputStream.write(String.format("%.5f ", v).getBytes());
                    }

                    fileOutputStream.close();

                } catch (Exception e) {
                    Log.e("PuddleJumper", "Unhandled exception:" + e.getMessage());
                }
            }
        });
    }

    public native void startCapture();
    public native void stopCapture();

    public native void startFMCW();
    public native void stopFMCW();

    public native float [] recordFor(int timeout_millis);
}
