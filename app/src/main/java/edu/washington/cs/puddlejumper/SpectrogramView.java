package edu.washington.cs.puddlejumper;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.util.Log;
import android.view.SurfaceView;
import android.view.SurfaceHolder;

public class SpectrogramView extends SurfaceView implements SurfaceHolder.Callback, Runnable {


    public SpectrogramView(Context context) {
        super(context);
    }

    public SpectrogramView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public SpectrogramView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    public SpectrogramView(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
    }

    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        // stub
    }

    public void surfaceCreated(SurfaceHolder holder) {
        // stub
    }

    public void surfaceDestroyed(SurfaceHolder holder) {
        // stub
    }

    @Override
    public void run() {
        SurfaceHolder holder = getHolder();

        float global_max = 0;

        while (true) {
            Canvas c = holder.lockCanvas();
            if (c == null) { continue; }
            c.drawARGB(255, 255, 255, 255);

            float [] magnitudes = getMagnitudes();

            float local_max = 0;
            for(int i = 0; i < magnitudes.length; ++i) {
                if (magnitudes[i] > local_max) {
                    local_max = magnitudes[i];
                }
            }
            if(local_max > global_max) {
                global_max = local_max;
            }

            int height = c.getHeight();
            int width = c.getWidth();
            float step = (float)width / (float)magnitudes.length;
            float scale = height/global_max;

            Paint p = new Paint();
            p.setARGB(255,0,0,0);
            for (int i = 0; i < magnitudes.length; ++i) {
                c.drawLine(i*step, height,i*step,height-magnitudes[i]*scale, p);
            }

            holder.unlockCanvasAndPost(c);
        }
    }

    public native float [] getMagnitudes();
}
