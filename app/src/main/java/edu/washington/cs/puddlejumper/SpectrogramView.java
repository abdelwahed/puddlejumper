package edu.washington.cs.puddlejumper;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Rect;
import android.util.AttributeSet;
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

        float globalMax = 0;

        float [] magnitudes = getMagnitudes();

        Canvas c = holder.lockCanvas();
        while(c == null) {
            c = holder.lockCanvas();
        }

        int height = c.getHeight();
        int width = c.getWidth();
        holder.unlockCanvasAndPost(c);

        int [] spectrogram = new int[magnitudes.length * width];
        int currentColumn = 0;

        while (true) {

            magnitudes = getMagnitudes();

            float localMax = 0;
            for(int i = 0; i < magnitudes.length; ++i) {
                if (magnitudes[i] > localMax) {
                    localMax = magnitudes[i];
                }
            }
            if(localMax > globalMax) {
                globalMax = localMax;
            }

            for(int i = 0; i < magnitudes.length; ++i) {
                int index = i * width + currentColumn;
                float normedMag = magnitudes[i] / globalMax * 360f;
                int color = Color.HSVToColor(new float[]{normedMag,1,1});
                spectrogram[index] = color;
                if(currentColumn < width - 1) {
                    spectrogram[index+1] = Color.parseColor("black");
                }
            }
            currentColumn = (currentColumn + 1) % width;
            Bitmap bmp = Bitmap.createBitmap(spectrogram, width, magnitudes.length, Bitmap.Config.ARGB_8888);

            c = holder.lockCanvas();
            if(c == null) {
                continue;
            }
            c.drawBitmap(bmp, null, new Rect(0, 0, width, height), null);
            holder.unlockCanvasAndPost(c);
        }
    }

    public native float [] getMagnitudes();
}
