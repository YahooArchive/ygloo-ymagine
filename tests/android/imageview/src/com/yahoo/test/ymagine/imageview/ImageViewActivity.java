
package com.yahoo.test.ymagine.imageview;

import android.app.Activity;
import android.content.res.AssetManager;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.ImageView;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;
import android.widget.TextView;

import com.yahoo.ymagine.Shader;
import com.yahoo.ymagine.Vbitmap;
import com.yahoo.mobile.client.android.ymagine.BitmapFactory;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class ImageViewActivity extends Activity {
    Button button;
    ImageView imageView;
    TextView messageView;
    TextView blurTextView;
    AssetManager assetMgr;
    SeekBar seekBar;

    private int mBlurRadius = 1;
    private Shader mShader = null;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        assetMgr = this.getAssets();
        addListenerOnButton();
        seekBar.setProgress(20);
        // seekBar.incrementProgressBy(5);

        /* Create collection of shaders */
        Vbitmap vbitmap = new Vbitmap();

        mShader = new Shader();
        mShader.brightness(0.2f);
        mShader.vignette(vbitmap, 0);
    }

    public void setMessage(String s) {
        if (messageView != null) {
            messageView.setText(s);
        }
    }

    public void setBlur(int r) {
        mBlurRadius = r;
        if (blurTextView != null) {
            blurTextView.setText(mBlurRadius + "px");
        }
    }

    public Bitmap loadTestImage(boolean usenative, OutputStream outstream) {
        String message = "";
        long start = 0;
        long elapsed = 0;
        long start_blur = 0;
        long elapsed_blur = 0;
        long start_quantize = 0;
        long elapsed_quantize = 0;
        long start_colorize = 0;
        long elapsed_colorize = 0;
        int niter = 1;
        int reqWidth = 256;
        int reqHeight = reqWidth;
        boolean justBounds = false;
        boolean runBlur = false;
        boolean runQuantize = false;
        boolean runColorize = false;
        int colors[];

        setMessage("Loading");

        CheckBox checkBox = (CheckBox) findViewById(R.id.checkbox_bounds);
        if (checkBox.isChecked()) {
            justBounds = true;
        }

        checkBox = (CheckBox) findViewById(R.id.checkbox_blur);
        if (checkBox.isChecked()) {
            runBlur = true;
        }

        checkBox = (CheckBox) findViewById(R.id.checkbox_quantize);
        if (checkBox.isChecked()) {
            runQuantize = true;
        }

        checkBox = (CheckBox) findViewById(R.id.checkbox_colorize);
        if (checkBox.isChecked()) {
            runColorize = true;
        }
        
        if (justBounds) {
            reqWidth = -1;
            reqHeight = -1;
        }

        Bitmap inbitmap = null;
        Bitmap outbitmap = null;
        InputStream instream = null;

        if (reqWidth > 0 && reqHeight > 0 && outstream == null) {
            inbitmap = Bitmap.createBitmap(reqWidth, reqHeight, Bitmap.Config.ARGB_8888);
            // inbitmap.eraseColor(Color.TRANSPARENT);
        }

        BitmapFactory.Options opts = new BitmapFactory.Options();

        if (inbitmap != null) {
            opts.inMaxWidth = inbitmap.getWidth();
            opts.inMaxHeight = inbitmap.getHeight();
            opts.inBitmap = inbitmap;
        } else if (outstream != null) {
            opts.inMaxWidth = reqWidth;
            opts.inMaxHeight = reqHeight;
        }
        opts.inCrop = false;
        opts.inKeepRatio = true;
        opts.inNative = usenative;
        opts.inShader = mShader;
        if (justBounds) {
            opts.inJustDecodeBounds = true;
        }

        elapsed = 0;

        // Use one of sample images from assets
        String filename = "sample1.jpg";
        filename = "sample1.webp";
        filename = "1_webp_ll.webp";

        colors = null;
        for (int i = 0; i < niter; i++) {
            try {
                instream = assetMgr.open(filename);
            } catch (IOException e1) {
            }
            if (instream == null) {
                break;
            }
            opts.inStream = outstream;

            start = System.nanoTime();
            outbitmap = BitmapFactory.decodeStream(instream, null, opts);
            elapsed += System.nanoTime() - start;

            try {
                instream.close();
            } catch (IOException e) {
            }

            if (runBlur) {
                start_blur = System.nanoTime();
                outbitmap = BitmapFactory.blur(outbitmap, mBlurRadius);
                elapsed_blur += System.nanoTime() - start_blur;
            }
            if (runQuantize) {
                start_quantize = System.nanoTime();
                colors = BitmapFactory.quantize(outbitmap, 8);
                elapsed_quantize += System.nanoTime() - start_quantize;
            }
            if (runColorize) {
                start_colorize = System.nanoTime();
                int color = BitmapFactory.getThemeColor(outbitmap);
                BitmapFactory.colorize(outbitmap, color);
                elapsed_colorize += System.nanoTime() - start_colorize;                
            }
        }

        if (niter > 0) {
            message = "decode in " + (elapsed / niter) / 1000 + "\u00b5s";
            if (opts !=  null && justBounds) {
                message += "\nsource bounds are " + opts.outWidth + "x" + opts.outHeight;
            }
            if (outbitmap != null) {
                message += "\nimage is " + outbitmap.getWidth() + "x"
                        + outbitmap.getHeight();
            }
            if (runBlur) {
                message += "\nblur in " + (elapsed_blur / niter) / 1000
                        + "\u00b5s";
            }
            if (runQuantize) {
                message += "\nquantize in " + (elapsed_quantize / niter) / 1000
                        + "\u00b5s";
                if (colors == null) {
                    message += " (null color)";
                } else {
                    message += " (" + colors.length + " colors)";
                }
            }
            if (runColorize) {
                message += "\ncolorize in " + (elapsed_colorize / niter) / 1000
                        + "\u00b5s"; 
            }
            if (opts.outPanoMode != 0) {
                message += "\nXMP found";
                message += " cropped=" + opts.outPanoCroppedWidth;
                message += "x" +opts.outPanoCroppedHeight;
                message += " full=" + opts.outPanoFullWidth;
                message += "x" + opts.outPanoFullHeight;
                message += " offset=" + opts.outPanoX;
                message += "," + opts.outPanoY;
            }

            setMessage(message);
        }

        return outbitmap;
    }

    public void addListenerOnButton() {
        messageView = (TextView) findViewById(R.id.messageView);
        blurTextView = (TextView) findViewById(R.id.blurTextView);
        imageView = (ImageView) findViewById(R.id.imageView1);
        seekBar = (SeekBar) findViewById(R.id.radiusSeek);

        seekBar.setMax(99);

        seekBar.setOnSeekBarChangeListener(new OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar arg0, int arg1, boolean arg2) {
                if (arg1 == 0) {
                    arg1 = 1;
                }
                setBlur(arg1);
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
            }
        });

        button = (Button) findViewById(R.id.btnLoadNative);
        button.setOnClickListener(new OnClickListener() {
            public void onClick(View arg0) {
                Bitmap bitmap = loadTestImage(true, null);
                if (bitmap != null) {
                    imageView.setImageBitmap(bitmap);
                } else {
                    imageView.setImageResource(R.drawable.nophoto);
                }
            }
        });

        button = (Button) findViewById(R.id.btnLoadSystem);
        button.setOnClickListener(new OnClickListener() {
            public void onClick(View arg0) {
                Bitmap bitmap = loadTestImage(false, null);
                if (bitmap != null) {
                    imageView.setImageBitmap(bitmap);
                } else {
                    imageView.setImageResource(R.drawable.nophoto);
                }
            }
        });

        button = (Button) findViewById(R.id.btnTranscode);
        button.setOnClickListener(new OnClickListener() {
            public void onClick(View arg0) {
                OutputStream os = null;
                ByteArrayOutputStream memoryOut = null;
                FileOutputStream fileOut = null;
                File tmpfile = null;
                boolean inmemory = true;

                if (inmemory) {
                    memoryOut = new ByteArrayOutputStream();
                    os = (OutputStream) memoryOut;
                } else {
                    final String tmpname = "test.jpg";
                    tmpfile = new File(getFilesDir(), tmpname);

                    /*
                     * Delete never throws exception, and we don't care about
                     * failure here
                     */
                    if (tmpfile.exists()) {
                        if (!tmpfile.delete()) {
                            /* Deletion failed, abort */
                            tmpfile = null;
                        }
                    }
                    if (tmpfile != null) {
                        try {
                            fileOut = openFileOutput(tmpfile.getAbsolutePath(), 0);
                            os = (OutputStream) fileOut;
                        } catch (FileNotFoundException e) {
                            tmpfile = null;
                        }
                    }
                }

                Bitmap bitmap = null;

                if (os != null) {
                    loadTestImage(true, os);

                    if (memoryOut != null) {
                        // System.out.println(String.format("Transcoded input in %d bytes", memoryOut.size()));
                        try {
                            memoryOut.close();
                        } catch (IOException e) {
                            // do nothing
                        }
                    }
                    if (fileOut != null) {
                        try {
                            fileOut.close();
                        } catch (IOException e) {
                            // do nothing
                        }
                    }

                    /* Try to load generated image into bitmap for verification */
                    if (inmemory) {
                        if (memoryOut != null) {
                            byte[] outData = memoryOut.toByteArray();
                            BitmapFactory.Options opts = new BitmapFactory.Options();
                            bitmap = BitmapFactory.decodeByteArray(outData, 0, outData.length, opts);
                        }
                    } else {
                        FileInputStream instream = null;
                        BitmapFactory.Options opts = new BitmapFactory.Options();
                        try {
                            instream = openFileInput(tmpfile.getAbsolutePath());
                            bitmap = BitmapFactory.decodeStream(instream, null, opts);
                        } catch (FileNotFoundException e) {
                            /* Ignore */
                        } finally {
                            if (instream != null) {
                                try {
                                    instream.close();
                                } catch (IOException e) {
                                    /* Ignore */
                                }
                            }
                        }
                    }
                }

                if (bitmap != null) {
                    imageView.setImageBitmap(bitmap);
                } else {
                    imageView.setImageResource(R.drawable.nophoto);
                }
            }
        });
    }
}
