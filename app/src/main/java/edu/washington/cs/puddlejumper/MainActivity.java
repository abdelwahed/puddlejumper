package edu.washington.cs.puddlejumper;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.widget.CompoundButton;
import android.widget.ToggleButton;

public class MainActivity extends Activity {

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    @Override
    protected void onStart() {
        super.onStart();

        if(checkSelfPermission(Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) {
            startCapture();
        }
    }

    @Override
    protected void onStop() {
        super.onStop();
        stopCapture();
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
    }

    public native void startCapture();
    public native void stopCapture();

    public native void startFMCW();
    public native void stopFMCW();
}
