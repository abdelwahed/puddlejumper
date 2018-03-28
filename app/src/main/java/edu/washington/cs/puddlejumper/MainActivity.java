package edu.washington.cs.puddlejumper;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.view.WindowManager;
import android.widget.CompoundButton;
import android.widget.ToggleButton;

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
    }

    private boolean checkAudioPermission() {
        int perm = checkSelfPermission(Manifest.permission.RECORD_AUDIO);
        return perm == PackageManager.PERMISSION_GRANTED;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        if(!checkAudioPermission()) {
            requestPermissions(new String[]{Manifest.permission.RECORD_AUDIO}, 0);
        }

        SpectrogramView specView = findViewById(R.id.spectrogramView);
        final Thread specThread = new Thread(specView);
        specThread.start();

        ToggleButton fmcwButton = findViewById(R.id.toggleButton);
        fmcwButton.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton compoundButton, boolean isChecked) {
                if(!checkAudioPermission()) {
                    compoundButton.setChecked(false);
                    return;
                }

                if(isChecked) {
                    startCapture();
                    startFMCW();
                } else {
                    stopFMCW();
                    stopCapture();
                    specThread.interrupt();
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
