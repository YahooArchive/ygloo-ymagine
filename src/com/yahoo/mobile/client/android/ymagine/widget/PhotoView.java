
package com.yahoo.mobile.client.android.ymagine.widget;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.BitmapShader;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ComposeShader;
import android.graphics.LinearGradient;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Paint.FontMetrics;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.PorterDuff;
import android.graphics.PorterDuff.Mode;
import android.graphics.PorterDuffXfermode;
import android.graphics.Rect;
import android.graphics.Shader;
import android.graphics.Shader.TileMode;
import android.os.Build;
import android.os.Handler;
import android.util.AttributeSet;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnTouchListener;
import android.view.ViewConfiguration;
import android.widget.Scroller;

import com.yahoo.mobile.client.android.ymagine.BitmapFactory;

public class PhotoView extends View implements OnTouchListener {
    private static final String LOG_TAG = "PhotoView";

    // private static final int video_id = R.drawable.video;
    private static Bitmap mVideoBitmap = null;
    private Bitmap mPhoto = null;

    private Context mContext;
    private Object mPhotoLock = new Object();
    private boolean mKeepBitmap = true;
    private boolean mVisible = false;
    private boolean mEditable = false;

    /* Handler to execute code into UI thread */
    private Handler mHandler = new Handler();
    /* Unique Id to detect when view has been recycled with a different photo */
    private long mUniqueId = 0;
    /* Current content */
    private boolean mSelected = false;
    private boolean mIsVideo = false;
    private boolean mNeedUpdateMatrix = false;

    private int mTextSize = 0;
    private int mTextHeight = 0;
    private int mTextAscent = 0;
    private int mTextDescent = 0;
    private int mTextPadVertical = 0;
    private int mTextPadHorizontal = 0;
    private Paint mTextPaint = null;
    private Paint mSolidPaint = null;

    private Paint mBitmapPaint = null;
    private Paint mGradientPaint = null;
    private Paint mSelectedPaint = null;

    private Rect mDrawRect = null;
    private Rect mSrcRect = null;
    private int mAlpha = 0xff;

    private String mTitle = null;

    public static final int GROW = 0;
    public static final int SHRINK = 1;

    public static final int DURATION = 100;
    public static final int TOUCH_INTERVAL = 100;

    public static final float MIN_SCALE = 0.5f;
    public static final float MAX_SCALE = 2.75f;
    public static final float ZOOM = 0.25f;

    // private static int _interpolator =
    // android.R.anim.accelerate_interpolator;

    private Matrix mMatrix = new Matrix();
    private Matrix mSavedMatrix = new Matrix();
    private Matrix mReflectionMatrix = new Matrix();

    private static final int MODE_NONE = 0;
    private static final int MODE_DRAG = 1;
    private static final int MODE_ZOOM = 2;

    private int mode = MODE_NONE;

    private PointF mStartPoint = new PointF();
    private PointF mMiddlePoint = new PointF();
    private Point mBitmapMiddlePoint = new Point();

    private float oldDist = 1f;
    private float matrixValues[] = {
            0f, 0f, 0f, 0f, 0f, 0f, 0f, 0f, 0f
    };
    private float scale;
    private float oldEventX = 0;
    private float oldEventY = 0;
    private float oldStartPointX = 0;
    private float oldStartPointY = 0;
    protected int mViewWidth = -1;
    protected int mViewHeight = -1;
    private int mBitmapWidth = -1;
    private int mBitmapHeight = -1;
    private boolean mDraggable = false;

    void updateMatrix() {
        mNeedUpdateMatrix = true;
    }

    void setImageMatrix(Matrix m) {
        mMatrix = m;
        postInvalidate();
        return;
    }

    @Override
    public boolean onTouch(View v, MotionEvent event) {
        if (!mEditable) {
            return false;
        }

        switch (event.getAction() & MotionEvent.ACTION_MASK) {
            case MotionEvent.ACTION_DOWN:
                mSavedMatrix.set(mMatrix);
                mStartPoint.set(event.getX(), event.getY());
                mode = MODE_DRAG;
                break;
            case MotionEvent.ACTION_POINTER_DOWN:
                oldDist = spacing(event);
                if (oldDist > 10f) {
                    mSavedMatrix.set(mMatrix);
                    midPoint(mMiddlePoint, event);
                    mode = MODE_ZOOM;
                }
                break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
                mode = MODE_NONE;
                break;
            case MotionEvent.ACTION_MOVE:
                if (mode == MODE_DRAG) {
                    drag(event);
                } else if (mode == MODE_ZOOM) {
                    zoom(event);
                }
                break;
        }

        return true;
    }

    public void drag(MotionEvent event) {
        mMatrix.getValues(matrixValues);

        float left = matrixValues[2];
        float top = matrixValues[5];
        float bottom = (top + (matrixValues[0] * mBitmapHeight)) - mViewHeight;
        float right = (left + (matrixValues[0] * mBitmapWidth)) - mViewWidth;

        float eventX = event.getX();
        float eventY = event.getY();
        float spacingX = eventX - mStartPoint.x;
        float spacingY = eventY - mStartPoint.y;
        float newPositionLeft = (left < 0 ? spacingX : spacingX * -1) + left;
        float newPositionRight = (spacingX) + right;
        float newPositionTop = (top < 0 ? spacingY : spacingY * -1) + top;
        float newPositionBottom = (spacingY) + bottom;
        boolean x = true;
        boolean y = true;

        if (newPositionRight < 0.0f || newPositionLeft > 0.0f) {
            if (newPositionRight < 0.0f && newPositionLeft > 0.0f) {
                x = false;
            } else {
                eventX = oldEventX;
                mStartPoint.x = oldStartPointX;
            }
        }
        if (newPositionBottom < 0.0f || newPositionTop > 0.0f) {
            if (newPositionBottom < 0.0f && newPositionTop > 0.0f) {
                y = false;
            } else {
                eventY = oldEventY;
                mStartPoint.y = oldStartPointY;
            }
        }

        if (mDraggable) {
            mMatrix.set(mSavedMatrix);
            mMatrix.postTranslate(x ? eventX - mStartPoint.x : 0, y ? eventY
                    - mStartPoint.y : 0);
            setImageMatrix(mMatrix);

            if (x)
                oldEventX = eventX;
            if (y)
                oldEventY = eventY;
            if (x)
                oldStartPointX = mStartPoint.x;
            if (y)
                oldStartPointY = mStartPoint.y;
        }

    }

    public void zoom(MotionEvent event) {
        mMatrix.getValues(matrixValues);

        float newDist = spacing(event);
        float bitmapWidth = matrixValues[0] * mBitmapWidth;
        float bimtapHeight = matrixValues[0] * mBitmapHeight;
        boolean in = newDist > oldDist;

        if (!in && matrixValues[0] < 1) {
            return;
        }
        if (bitmapWidth > mViewWidth || bimtapHeight > mViewHeight) {
            mDraggable = true;
        } else {
            mDraggable = false;
        }

        float midX = (mViewWidth / 2);
        float midY = (mViewHeight / 2);

        mMatrix.set(mSavedMatrix);
        scale = newDist / oldDist;
        mMatrix.postScale(scale, scale,
                bitmapWidth > mViewWidth ? mMiddlePoint.x : midX,
                bimtapHeight > mViewHeight ? mMiddlePoint.y : midY);

        setImageMatrix(mMatrix);
    }

    /** Determine the space between the first two fingers */
    private float spacing(MotionEvent event) {
        float x = event.getX(0) - event.getX(1);
        float y = event.getY(0) - event.getY(1);

        return (float) Math.sqrt(x * x + y * y);
    }

    /** Calculate the mid point of the first two fingers */
    private void midPoint(PointF point, MotionEvent event) {
        float x = event.getX(0) + event.getX(1);
        float y = event.getY(0) + event.getY(1);
        point.set(x / 2, y / 2);
    }

    PhotoView im = null;

    float xCur, yCur, xPre, yPre, xSec, ySec, distDelta, distCur, distPre;
    float xScale = 1.0f;
    float yScale = 1.0f;
    int mTouchSlop;
    long mLastGestureTime;
    Paint mPaint;
    Scroller mScroller;

    public PhotoView(Context context) {
        super(context);
        init(context);
    }

    public PhotoView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init(context);
    }

    public PhotoView(Context context, AttributeSet attrs, int defaultStyle) {
        super(context, attrs);
        init(context);
    }

    private void init(Context context) {
        mContext = context;

        ViewConfiguration vc = ViewConfiguration.get(context);

        im = this;
        mTouchSlop = vc.getScaledTouchSlop();
        mPaint = new Paint();
        mPaint.setAntiAlias(true);
        mScroller = new Scroller(getContext());
        mVisible = this.isShown();

        mReflectionMatrix.preScale(1, -1);
        
        this.setOnTouchListener(this);
    }

    @Override
    @TargetApi(Build.VERSION_CODES.HONEYCOMB)
    public void setAlpha(float alpha) {
        int newAlpha;

        if (alpha <= 0.0f) {
            newAlpha = 0;
        } else if (alpha >= 1.0f) {
            newAlpha = 0xff;
        } else {
            newAlpha = ((int) (alpha * 255.0f));
        }

        if (newAlpha != mAlpha) {
            mAlpha = newAlpha;
            postInvalidate();
        }
    }

    public void setTextSize(int size) {
        if (size < 0) {
            size = 0;
        }
        if (size == mTextSize) {
            return;
        }

        if (size == 0) {
            if (mTextPaint != null) {
                mTextPaint.reset();
            }

            mTextPaint = null;
            mTextSize = 0;
            mTextAscent = 0;
            mTextDescent = 0;
            mTextHeight = 0;
            mTextPadVertical = 0;
            mTextPadHorizontal = 0;

            return;
        }

        if (mTextPaint == null) {
            mTextPaint = new Paint();
        } else {
            mTextPaint.reset();
        }

        mTextSize = size;

        mTextPaint.setTextAlign(Paint.Align.LEFT);
        mTextPaint.setColor(Color.BLACK);
        mTextPaint.setTextSize(mTextSize);

        FontMetrics metrics = mTextPaint.getFontMetrics();

        mTextAscent = Math.abs((int) metrics.ascent);
        mTextDescent = Math.abs((int) metrics.descent);
        mTextHeight = Math.max(mTextAscent + mTextDescent, 1);
        mTextPadVertical = Math.max(mTextHeight / 6, 1);
        mTextPadHorizontal = Math.max(mTextHeight / 3, 1);
    }

    public void setImageBitmap(Bitmap photo) {
        synchronized(mPhotoLock) {
//            /* Remember reference requirement */
//            boolean keepBak = mKeepBitmap;
//
//            /* Unregister previous photo container */
//            if (mPhoto != null) {
//                mPhoto.removeSubscriber(this);
//                mPhoto.releaseBitmap(mRefBitmap);
//                mPhoto.release();
//                
//                mPhoto = null;
//                mRefBitmap = null;
//            }
//            
//            /* Create unique Id for updated view */
//            mUniqueId++;
//            mPhoto = photo;
//            mKeepBitmap = keepBak;
//
//            if (mPhoto != null) {
//                mPhoto.retain();
//                mPhoto.addSubscriber(this);
//                // setImageMatrix(mMatrix);
//                
//                if (mKeepBitmap) {
//                    mRefBitmap = mPhoto.retainBitmap();
//                }
//            }
            mPhoto = photo;
        }
        onPhotoChanged();
    }

    public void setImageResource(int resId) {
        Bitmap bitmap = BitmapFactory.decodeResource(getResources(), resId);
        setImageBitmap(bitmap);
    }

    @Override
    public void setSelected(boolean selected) {
        mSelected = selected;
    }

    public void reset() {
        setImageBitmap(null);

        /* Reset view status */
        mSelected = false;
    }

    private void drawContent(Canvas canvas) {
        if (mAlpha <= 0) {
            /* Full transparent window, ignore */
            return;
        }

        int w = canvas.getWidth();
        int h = canvas.getHeight();

        if (mDrawRect == null) {
            mDrawRect = new Rect(0, 0, 0, 0);
        }
        if (mSrcRect == null) {
            mSrcRect = new Rect(0, 0, 0, 0);
        }
        if (mSolidPaint == null) {
            mSolidPaint = new Paint();
        }

        int padTop = this.getPaddingTop();
        int padBottom = this.getPaddingBottom();
        int padLeft = this.getPaddingLeft();
        int padRight = this.getPaddingRight();

        int cw = w - padLeft - padRight;
        int ch = h - padTop - padBottom;
        if (cw <= 0 || ch <= 0) {
            return;
        }

        Bitmap bitmap = null;
        synchronized(mPhotoLock) {
            bitmap = mPhoto;
//            refbitmap = mRefBitmap;            
//            if (photo != null) {
//                photo.retain();
//            }
//            if (refbitmap != null) {
//                refbitmap.retain(ReferenceType.DISPLAY);
//                bitmap = refbitmap.getBitmap(false);
//            }
//            if (bitmap == null) {
//                if (photo != null) {
//                    photo.prefetch(240,  240);
//                }
//            }
        }

        int bw;
        int bh;
        int destw;
        int desth;

        if (bitmap != null) {
            /* Actual size of the photo to render */
            bw = bitmap.getWidth();
            bh = bitmap.getHeight();
//        } else if (photo != null) {
//            /* Information about the aspect ratio of the photo */
//            bw = photo.getWidth();
//            bh = photo.getHeight();
        } else {
            bw = cw;
            bh = ch;
        }

        if (mNeedUpdateMatrix) {
            mNeedUpdateMatrix = false;

            mBitmapWidth = bw;
            mBitmapHeight = bh;
            mBitmapMiddlePoint.x = (mViewWidth / 2) - (mBitmapWidth / 2);
            mBitmapMiddlePoint.y = (mViewHeight / 2) - (mBitmapHeight / 2);

            mMatrix.postTranslate(mBitmapMiddlePoint.x, mBitmapMiddlePoint.y);
        }

        boolean letterbox = true;
        if (letterbox) {
            destw = cw;
            desth = (bh * cw) / bw;

            int destx = padLeft + (cw - destw) / 2;
            int desty = padTop + (ch - desth) / 2;

            mDrawRect.set(destx, desty, destx + destw, desty + desth);

            if (bitmap != null) {
                mSrcRect.set(0, 0, bw, bh);
            }
        } else {
            /* Zoom in (and cropped) */
            destw = cw;
            desth = ch;

            mSrcRect.set(0, 0, bitmap.getWidth(), bitmap.getHeight());
            mDrawRect.set(padLeft, padTop, padLeft + cw, padTop + ch);
        }

        // fill only regions of the canvas with no other (opaque) content */
        boolean fillBackground = false;
        boolean showTitle = false;
        if (mAlpha == 0xff) {
            showTitle = true;
        }
        if (fillBackground) {
            mSolidPaint.setColor(Color.argb(mAlpha, 0x00, 0x00, 0x00));
            if (mDrawRect.top > padTop) {
                canvas.drawRect(padLeft, padTop, padLeft + cw, mDrawRect.top,
                        mSolidPaint);
            }
            if (mDrawRect.bottom < padTop + ch) {
                canvas.drawRect(padLeft, mDrawRect.bottom, padLeft + cw, padTop
                        + ch, mSolidPaint);
            }
            if (mDrawRect.left > padLeft) {
                canvas.drawRect(padLeft, mDrawRect.top, mDrawRect.left,
                        mDrawRect.bottom, mSolidPaint);
            }
            if (mDrawRect.right < padLeft + cw) {
                canvas.drawRect(mDrawRect.right, mDrawRect.top, padLeft + cw,
                        mDrawRect.bottom, mSolidPaint);
            }
        }

        if (bitmap != null) {
            boolean drawReflection = false;

            if (mBitmapPaint == null) {
                mBitmapPaint = new Paint();
            }
            mBitmapPaint.reset();
            if (mAlpha < 0xff) {
                mBitmapPaint.setAlpha(mAlpha);
            }

            if (drawReflection) {
                mMatrix.reset();
                mMatrix.postScale(((float) mDrawRect.width()) / mSrcRect.width(), ((float) mDrawRect.height() * 0.5f) / mSrcRect.height(),  0,  0);
                mMatrix.postTranslate(mDrawRect.left, mDrawRect.top);
                canvas.drawBitmap(bitmap, mMatrix, mBitmapPaint);

                //Create a Bitmap with the flip matix applied to it.
                //We only want the bottom half of the image
                Matrix matrix = new Matrix();
                matrix.preScale(1, -1);

                Bitmap reflectionImage = Bitmap.createBitmap(bitmap, 0, bitmap.getHeight()/2, bitmap.getWidth(), bitmap.getHeight()/2, matrix, false);
                reflectionImage = BitmapFactory.blur(reflectionImage, 20, 200, 200, true);
                mReflectionMatrix.reset();
                mReflectionMatrix.postScale(((float) mDrawRect.width()) / reflectionImage.getWidth(), ((float) mDrawRect.height() * 0.5f) / reflectionImage.getHeight(),  0,  0);
                mReflectionMatrix.postTranslate(mDrawRect.left, mDrawRect.top + (mDrawRect.height() / 2));
                canvas.drawBitmap(reflectionImage, mReflectionMatrix, mBitmapPaint);
            
                /* Smooth transition image. Draw the blur image on top of the original one, with a linear gradient alpha channel */
                int transitionHeight = (reflectionImage.getHeight() * 2) / 10;
                if (transitionHeight > 1) {
                    Paint mTransitionPaint = null;
                    if (mTransitionPaint == null) {
                        mTransitionPaint = new Paint();
                    } else {
                        mTransitionPaint.reset();
                    }

                    mTransitionPaint.setStyle(Paint.Style.FILL);
                    mTransitionPaint.setAntiAlias(true);

                    final int shaderColor0 = Color.argb(0x00, 0x00, 0x00, 0x00);
                    final int shaderColor1 = Color.argb(0xff, 0xff, 0x00, 0x00);

                    LinearGradient gradient = new LinearGradient(0, reflectionImage.getHeight() - transitionHeight, reflectionImage.getWidth(), reflectionImage.getHeight(),
                                                                 shaderColor1, shaderColor0,
                                                                 Shader.TileMode.CLAMP);
                
                    BitmapShader mTransitionShader = new BitmapShader(reflectionImage, Shader.TileMode.CLAMP, Shader.TileMode.CLAMP);
                
                    mTransitionPaint.setShader(new ComposeShader(mTransitionShader, gradient, PorterDuff.Mode.SRC_OVER));
                
                    // canvas.drawBitmap(reflectionImage, mReflectionMatrix, mTranstionPaint);
                    canvas.drawBitmap(reflectionImage, mDrawRect.left, mDrawRect.top + (mDrawRect.height() / 2), mTransitionPaint);
                }
            } else {
                mMatrix.reset();
                mMatrix.postScale(((float) mDrawRect.width()) / mSrcRect.width(), ((float) mDrawRect.height()) / mSrcRect.height(),  0,  0);
                mMatrix.postTranslate(mDrawRect.left, mDrawRect.top);
                canvas.drawBitmap(bitmap, mMatrix, mBitmapPaint);
            }
            // canvas.drawBitmap(bitmap, mSrcRect, mDrawRect, mBitmapPaint);
        } else {
            if (mGradientPaint != null) {
                canvas.drawRect(mDrawRect, mGradientPaint);
            }
        }

        mTitle = null;
        mIsVideo = false;
//        if (photo != null) {
//            mTitle = photo.getTitle();
//            mIsVideo = photo.isVideo();
//        }

        if (showTitle) {
            if (mTitle != null) {
                if (mTextPaint != null) {
                    mSolidPaint.setColor(Color.argb(0x80, 0xff, 0xff, 0xff));
                    canvas.drawRect(padLeft, h - padBottom - mTextHeight - 2
                            * mTextPadVertical, w - padRight, h - padBottom,
                            mSolidPaint);

                    canvas.drawText(mTitle, padLeft + mTextPadHorizontal, h
                            - padBottom - mTextPadVertical - mTextDescent,
                            mTextPaint);
                }
            }
        }

        if (mIsVideo) {
            if (mVideoBitmap == null) {
                Resources res = mContext.getResources();
                // mVideoBitmap = BitmapFactory.decodeResource(res, video_id);
            }
            if (mVideoBitmap != null) {
                int vbw = mVideoBitmap.getWidth();
                int vbh = mVideoBitmap.getHeight();
                int cx = padLeft + (w - padLeft - padRight) / 2;
                int cy = padTop + (h - padTop - padBottom) / 2;
                int bmin = Math.min(vbw, vbh);

                mDrawRect.set(cx - bmin / 3, cy - bmin / 3, cx + bmin / 3, cy
                        + bmin / 3);
                canvas.drawBitmap(mVideoBitmap, null, mDrawRect, null);
            }
        }

        if (mSelected) {
            if (mSelectedPaint == null) {
                mSelectedPaint = new Paint();
                mSelectedPaint.setColor(Color.argb(0x80, 0x10, 0xc0, 0xff));
            }

            canvas.drawRect(padLeft, padTop, w - padRight, h - padBottom,
                    mSelectedPaint);
        }

//        if (refbitmap != null) {
//            refbitmap.release(ReferenceType.DISPLAY);
//        }
//        if (photo != null) {
//            photo.release();
//        }

        return;
    }

    @Override
    protected void onDraw(Canvas canvas) {
        drawContent(canvas);
    }

    @Override
    public void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);

        int padTop = this.getPaddingTop();
        int padBottom = this.getPaddingBottom();
        int padLeft = this.getPaddingLeft();
        int padRight = this.getPaddingRight();

        mViewWidth = w - padLeft - padRight;
        mViewHeight = h - padTop - padBottom;

        updateMatrix();

        if (w == oldw && h == oldh) {
            return;
        }

        if (w <= 0 || h <= 0) {
            mGradientPaint = null;
            return;
        }

        if (mGradientPaint == null) {
            mGradientPaint = new Paint();
        } else {
            mGradientPaint.reset();
        }

        mGradientPaint.setStyle(Paint.Style.FILL);
        mGradientPaint.setAntiAlias(true);

        final int shaderColor0 = Color.argb(0xff, 0x10, 0x10, 0x10);
        final int shaderColor1 = Color.argb(0xff, 0x40, 0x40, 0x40);

        Shader linearGradientShader = new LinearGradient(padLeft, padTop, w
                - padRight, h - padBottom, shaderColor1, shaderColor0,
                Shader.TileMode.CLAMP);

        mGradientPaint.setShader(linearGradientShader);
    }

    public void drawReflection(Canvas canvas, Bitmap bitmap) {
        //The gap we want between the reflection and the original image
        final int reflectionGap = 4;
        
        int width = bitmap.getWidth();
        int height = bitmap.getHeight();
        
       
        //This will not scale but will flip on the Y axis
        Matrix matrix = new Matrix();
        matrix.preScale(1, -1);
        
        //Create a Bitmap with the flip matix applied to it.
        //We only want the bottom half of the image
        Bitmap reflectionImage = Bitmap.createBitmap(mPhoto, 0, height/2, width, height/2, matrix, false);
            
        //Create a new bitmap with same width but taller to fit reflection
        Bitmap bitmapWithReflection = Bitmap.createBitmap(width, (height + height/2), Config.ARGB_8888);
      
       //Create a new Canvas with the bitmap that's big enough for
       //the image plus gap plus reflection
       // Canvas canvas = new Canvas(bitmapWithReflection);
        
       //Draw in the original image
       canvas.drawBitmap(bitmap, 0, 0, null);
       //Draw in the gap
       Paint defaultPaint = new Paint();
       canvas.drawRect(0, height, width, height + reflectionGap, defaultPaint);
       //Draw in the reflection
       canvas.drawBitmap(reflectionImage,0, height + reflectionGap, null);
       
       //Create a shader that is a linear gradient that covers the reflection
       Paint paint = new Paint(); 
       LinearGradient shader = new LinearGradient(0, mPhoto.getHeight(), 0, 
         bitmapWithReflection.getHeight() + reflectionGap, 0x70ffffff, 0x00ffffff, 
         TileMode.CLAMP); 
       //Set the paint to use this shader (linear gradient)
       paint.setShader(shader); 
       //Set the Transfer mode to be porter duff and destination in
       paint.setXfermode(new PorterDuffXfermode(Mode.DST_IN)); 
       //Draw a rectangle using the paint with our linear gradient
       canvas.drawRect(0, height, width, 
         bitmapWithReflection.getHeight() + reflectionGap, paint);       
    }

    @Override
    public void onDetachedFromWindow() {
        // reset();
    }

    /*
     * When onPhotoChanged is called, it is guaranteed that getBitmap() has a
     * valid reference to the actual bitmap
     */
    // @Override
    public void onPhotoChanged() {
        long id;

        synchronized(mPhotoLock) {
            id = mUniqueId;
            if (mPhoto != null) {
//                if (mRefBitmap != null) {
//                    mPhoto.releaseBitmap(mRefBitmap);
//                }
//                mRefBitmap = mPhoto.retainBitmap();
            }
        }

        try {
            BitmapDisplayer bd = new BitmapDisplayer(id);
            mHandler.post(bd);
        } catch (Throwable th) {
            // th.printStackTrace();
        }
    }

    // Used to display bitmap in the UI thread
    class BitmapDisplayer implements Runnable {
        long mReferenceId = 0;

        public BitmapDisplayer(long referenceid) {
            mReferenceId = referenceid;
        }

        @Override
        public void run() {
            synchronized(mPhotoLock) {
                if (mReferenceId == mUniqueId) {
                    invalidate();
                }
            }
        }
    }

    @Override
    public void onVisibilityChanged(View changedView, int visibility) {
        super.onVisibilityChanged(changedView, visibility);

        if (visibility == VISIBLE) {
            mVisible = true;
        } else {
            mVisible = false;
            //releaseReference();
        }
    }

    @Override
    public void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);

        if (visibility == VISIBLE) {
            mVisible = true;
        } else {
            mVisible = false;
            //releaseReference();
        }
    }

    @Override
    protected void finalize() throws Throwable {
        /*
         * Get sure all references to bitmap are released when view is
         * destroyed. Should normally already be destroyed when view is detached
         * from its parent window.
         */
        reset();

        super.finalize();
    }
}
