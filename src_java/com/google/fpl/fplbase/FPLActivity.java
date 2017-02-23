// Copyright 2015 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Helper class that allows us to rename our app.
// Can't just modify SDLActivity, since the native code depends on that package.

package com.google.fpl.fplbase;

import android.app.Activity;
import android.app.AlarmManager;
import android.app.AlertDialog;
import android.app.Instrumentation;
import android.app.PendingIntent;
import android.app.UiModeManager;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.graphics.Point;
import android.hardware.Sensor;
import android.hardware.SensorManager;
import android.media.AudioManager;
import android.nfc.NdefMessage;
import android.os.Bundle;
import android.text.Html;
import android.text.method.LinkMovementMethod;
import android.text.util.Linkify;
import android.util.Log;
import android.util.TypedValue;
import android.view.Choreographer;
import android.view.Display;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.OrientationEventListener;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.WindowManager;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.ScrollView;
import android.widget.TextView;
import com.google.vr.cardboard.UiLayer;
import com.google.vrtoolkit.cardboard.CardboardDeviceParams;
import com.google.vrtoolkit.cardboard.CardboardView;
import com.google.vrtoolkit.cardboard.Eye;
import com.google.vrtoolkit.cardboard.HeadTransform;
import com.google.vrtoolkit.cardboard.PhoneParams;
import com.google.vrtoolkit.cardboard.proto.Phone;
import com.google.vrtoolkit.cardboard.ScreenParams;
import com.google.vrtoolkit.cardboard.sensors.MagnetSensor;
import com.google.vrtoolkit.cardboard.sensors.NfcSensor;
import org.libsdl.app.SDLActivity;

public class FPLActivity extends SDLActivity implements
    MagnetSensor.OnCardboardTriggerListener, NfcSensor.OnCardboardNfcListener,
    Choreographer.FrameCallback {

  private static final float METERS_PER_INCH = 0.0254f;
  private final Instrumentation instrumentation = new Instrumentation();

  // Fields used in order to interact with a Cardboard device
  private CardboardView cardboardView;
  private MagnetSensor magnetSensor;
  private NfcSensor nfcSensor;
  private HeadTransform headTransform;
  private Eye leftEye;
  private Eye rightEye;
  private Eye monocularEye;
  private Eye leftEyeNoDistortion;
  private Eye rightEyeNoDistortion;
  private UiLayer uiLayer;
  private OrientationEventListener orientationListener;
  private int cachedDeviceRotation;

  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    nativeInitVsync();
    // Instantiate fields used by Cardboard, if we support HMDs.
    if (InitializeHeadMountedDisplayOnStart()) {
      cardboardView = new CardboardView(this);
      if (cardboardView != null) {
        headTransform = new HeadTransform();
        leftEye = new Eye(Eye.Type.LEFT);
        rightEye = new Eye(Eye.Type.RIGHT);
        monocularEye = new Eye(Eye.Type.MONOCULAR);
        leftEyeNoDistortion = new Eye(Eye.Type.LEFT);
        rightEyeNoDistortion = new Eye(Eye.Type.RIGHT);
        magnetSensor = new MagnetSensor(this);
        magnetSensor.setOnCardboardTriggerListener(this);
        nfcSensor = NfcSensor.getInstance(this);
        nfcSensor.addOnCardboardNfcListener(this);
        NdefMessage tagContents = nfcSensor.getTagContents();
        if (tagContents != null) {
          updateCardboardDeviceParams(CardboardDeviceParams.createFromNfcContents(tagContents));
        }
        orientationListener = CreateOrientationListener();
      } else {
        Log.w("SDL", "Failed to create CardboardView");
      }
    }
  }

  @Override
  public void onDestroy() {
    super.onDestroy();
    nativeCleanupVsync();
  }

  @Override
  public void onResume() {
    super.onResume();
    if (cardboardView != null) {
      cardboardView.onResume();
      magnetSensor.start();
      nfcSensor.onResume(this);
      orientationListener.enable();
    }
    Choreographer.getInstance().postFrameCallback(this);

    // Request audio focus for playback.
    // Purposefully not registering a callback here as the app should monitor
    // activity lifecycle events to pause / resume audio playback accordingly.
    Context context = getBaseContext();
    AudioManager am = (AudioManager)context.getSystemService(Context.AUDIO_SERVICE);
    am.requestAudioFocus(null,
                         AudioManager.STREAM_MUSIC,
                         AudioManager.AUDIOFOCUS_GAIN);
  }

  @Override
  public void onPause() {
    super.onPause();
    if (cardboardView != null) {
      cardboardView.onPause();
      magnetSensor.stop();
      nfcSensor.onPause(this);
      orientationListener.disable();
    }
    Choreographer.getInstance().removeFrameCallback(this);

    Context context = getBaseContext();
    AudioManager am = (AudioManager)context.getSystemService(Context.AUDIO_SERVICE);
    am.abandonAudioFocus(null);
  }

  // GPG's GUIs need activity lifecycle events to function properly, but
  // they don't have access to them. This code is here to route these events
  // back to GPG through our C++ code.
  protected void onActivityResult(int requestCode, int resultCode, Intent data) {
    super.onActivityResult(requestCode, resultCode, data);
    nativeOnActivityResult(this, requestCode, resultCode, data);
  }
  boolean textDialogOpen = false;
  int queryResponse = -1;

  protected boolean UseImmersiveMode() {
    final int BUILD_VERSION_KITCAT = 18;
    return android.os.Build.VERSION.SDK_INT >= BUILD_VERSION_KITCAT;
  }

  @Override
  public void onWindowFocusChanged(boolean hasFocus) {
    super.onWindowFocusChanged(hasFocus);
    if (hasFocus) {
      textDialogOpen = false;
    }
    if (UseImmersiveMode() && hasFocus) {
      // We use API 15 as our minimum, and these are the only features we
      // use in higher APIs, so we define cloned constants:
      final int SYSTEM_UI_FLAG_LAYOUT_STABLE = 256;  // API 16
      final int SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION = 512;  // API 16
      final int SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN = 1024;  // API 16
      final int SYSTEM_UI_FLAG_HIDE_NAVIGATION = 2;  // API 14
      final int SYSTEM_UI_FLAG_FULLSCREEN = 4;  // API 16
      final int SYSTEM_UI_FLAG_IMMERSIVE_STICKY = 4096; // API 19
      mLayout.setSystemUiVisibility(
          SYSTEM_UI_FLAG_LAYOUT_STABLE
          | SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
          | SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
          | SYSTEM_UI_FLAG_HIDE_NAVIGATION
          | SYSTEM_UI_FLAG_FULLSCREEN
          | SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
    }
  }

  private class TextDialogRunnable implements Runnable {
    Activity activity;
    String title;
    String text;
    boolean html;
    public TextDialogRunnable(Activity activity, String title, String text,
                              boolean html) {
      this.activity = activity;
      this.title = title;
      this.text = text;
      this.html = html;
    }

    private class LinkInterceptingWebViewClient extends WebViewClient {
      @Override
      public boolean shouldOverrideUrlLoading(WebView view, String url) {
        return false;
      }
    }

    public void run() {
      try {
        textDialogOpen = true;

        WebView webview = new WebView(activity);
        webview.setWebViewClient(new LinkInterceptingWebViewClient());
        webview.loadData(text, "text/html", null);
        AlertDialog alert = new AlertDialog.Builder(activity, AlertDialog.THEME_DEVICE_DEFAULT_DARK)
            .setTitle(title)
            .setView(webview)
            .setNeutralButton("OK", null)
            .create();
        alert.show();
      } catch (Exception e) {
        textDialogOpen = false;
        Log.e("SDL", "exception", e);
      }
    }
  }

  // A Runnable to display a query dialog, asking the user a yes-or-no question.
  // Sets queryResponse in the parent class to 0 or 1 when the user responds.
  private class QueryDialogRunnable implements Runnable {
    Activity activity;
    final String title;
    final String question;
    final String yes;
    final String no;
    public QueryDialogRunnable(Activity activity, String title, String question,
                               String yes, String no) {
      this.activity = activity;
      this.title = title;
      this.question = question;
      this.yes = yes;
      this.no = no;
    }

    public void run() {
      try {
        queryResponse = -1;
        textDialogOpen = true;

        AlertDialog alert = new AlertDialog.Builder(activity, AlertDialog.THEME_DEVICE_DEFAULT_LIGHT)
            .setTitle(title)
            .setMessage(question)
            .setNegativeButton(no, new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface dialog,int id) {
                  queryResponse = 0;
                  textDialogOpen = false;
                }})
            .setPositiveButton(yes, new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface dialog,int id) {
                  queryResponse = 1;
                  textDialogOpen = false;
                }})
            .create();
        alert.show();
      } catch (Exception e) {
        textDialogOpen = false;
        Log.e("SDL", "exception", e);
      }
    }
  }

  // Capture motionevents and keyevents to check for gamepad movement.  Any events we catch
  // (That look like they were from a gamepad or joystick) get sent to C++ via JNI, where
  // they are stored, so C++ can deal with them next time it updates the game state.
  @Override
  public boolean dispatchGenericMotionEvent(MotionEvent event) {
    if ((event.getAction() == MotionEvent.ACTION_MOVE) &&
       (event.getSource() & (InputDevice.SOURCE_JOYSTICK | InputDevice.SOURCE_GAMEPAD)) != 0) {
       float axisX = event.getAxisValue(MotionEvent.AXIS_X);
       float axisY = event.getAxisValue(MotionEvent.AXIS_Y);
       float hatX = event.getAxisValue(MotionEvent.AXIS_HAT_X);
       float hatY = event.getAxisValue(MotionEvent.AXIS_HAT_Y);
       float finalX, finalY;
       // Decide which values to send, based on magnitude.  Hat values, or analog/axis values?
       if (Math.abs(axisX) + Math.abs(axisY) > Math.abs(hatX) + Math.abs(hatY)) {
         finalX = axisX;
         finalY = axisY;
       } else {
         finalX = hatX;
         finalY = hatY;
       }
       nativeOnGamepadInput(event.getDeviceId(), event.getAction(),
                          0,  // Control Code is not needed for motionEvents.
                          finalX, finalY);
    }
    return super.dispatchGenericMotionEvent(event);
  }

  private boolean changingVolume = false;

  @Override
  public boolean dispatchKeyEvent(KeyEvent event)
  {
    if ((event.getSource() &
        (InputDevice.SOURCE_JOYSTICK | InputDevice.SOURCE_GAMEPAD)) != 0) {
      nativeOnGamepadInput(event.getDeviceId(), event.getAction(),
                           event.getKeyCode(), 0.0f, 0.0f);
    }
    int keyCode = event.getKeyCode();
    // Disable the volume keys while in a cardboard
    if (keyCode == KeyEvent.KEYCODE_VOLUME_DOWN || keyCode == KeyEvent.KEYCODE_VOLUME_UP) {
      if (nfcSensor != null && nfcSensor.isDeviceInCardboard()) {
        return true;
      }
      int action = event.getAction();
      if (action == KeyEvent.ACTION_DOWN) {
        changingVolume = true;
      } else if (action == KeyEvent.ACTION_UP) {
        changingVolume = false;
      }
    }
    return super.dispatchKeyEvent(event);
  }

  public void showTextDialog(String title, String text, boolean html) {
    runOnUiThread(new TextDialogRunnable(this, title, text, html));
  }

  public boolean isTextDialogOpen() {
    return textDialogOpen;
  }

  public void showQueryDialog(String title, String query_text, String yes_text, String no_text)
  {
    runOnUiThread(new QueryDialogRunnable(this, title, query_text, yes_text, no_text));
  }

  public int getQueryDialogResponse() {
    if (textDialogOpen)
      return -1;
    else
      return queryResponse;
  }

  public void resetQueryDialogResponse() {
    queryResponse = -1;
  }

  public boolean hasSystemFeature(String featureName) {
    return getPackageManager().hasSystemFeature(featureName);
  }

  public int DpToPx(int dp) {
    // Convert the dps to pixels, based on density scale
    return (int)TypedValue.applyDimension(
      TypedValue.COMPLEX_UNIT_DIP, dp, getResources().getDisplayMetrics());
  }

  public boolean IsPortrait() {
    return getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT;
  }

  public int[] GetDisplaySize() {
    Point size = new Point();
    // Immersive mode uses the full screen, so get the real size if using it
    if (UseImmersiveMode()) {
      getWindowManager().getDefaultDisplay().getRealSize(size);
    } else {
      getWindowManager().getDefaultDisplay().getSize(size);
    }

    int min = Math.min(size.x, size.y);
    int max = Math.max(size.x, size.y);
    return new int[] { IsPortrait() ? min : max, IsPortrait() ? max : min };
  }

  public void SetHeadMountedDisplayResolution(int width, int height) {
    // If hardware scaling is used, the width x height will be less than the
    // displays natural resolution, so the PPI (pixels per inch) will also
    // be different. So, we use this trick to recalculate the ScreenParam's PPI
    // values (which are normally just read from the display).
    try {
      if (cardboardView == null) return;
      Display display = getWindowManager().getDefaultDisplay();
      ScreenParams sp = new ScreenParams(display);
      Phone.PhoneParams pp = new Phone.PhoneParams();
      pp.setXPpi(width / sp.getWidthMeters() * METERS_PER_INCH);
      pp.setYPpi(height / sp.getHeightMeters() * METERS_PER_INCH);
      sp = ScreenParams.fromProto(display, pp);
      sp.setWidth(width);
      sp.setHeight(height);
      cardboardView.updateScreenParams(sp);
    } catch (Exception e) {
      Log.e("SDL", "exception", e);
    }
  }

  protected boolean InitializeHeadMountedDisplayOnStart() {
    SensorManager sensorManager = (SensorManager)getSystemService(Context.SENSOR_SERVICE);
    return sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE) != null &&
           sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER) != null;
  }

  public boolean SupportsHeadMountedDisplay() {
    // Only supports head mounted display if the SDK initialized correctly.
    return cardboardView != null;
  }

  @Override
  public void onCardboardTrigger() {
    nativeOnCardboardTrigger();
  }

  @Override
  public void onInsertedIntoCardboard(CardboardDeviceParams cardboardDeviceParams) {
    updateCardboardDeviceParams(cardboardDeviceParams);
    nativeSetDeviceInCardboard(true);
  }

  @Override
  public void onRemovedFromCardboard() {
    nativeSetDeviceInCardboard(false);
  }

  protected void updateCardboardDeviceParams(CardboardDeviceParams newParams) {
    cardboardView.updateCardboardDeviceParams(newParams);
  }

  // Returns true if the current device is a TV device, false otherwise.
  public boolean IsTvDevice() {
    UiModeManager uiModeManager = (UiModeManager)getSystemService(UI_MODE_SERVICE);
    return uiModeManager.getCurrentModeType() == Configuration.UI_MODE_TYPE_TELEVISION;
  }

  // Function to access the transforms of the eyes, which includes head tracking
  public void GetEyeViews(float[] headView, float[] leftTransform, float[] rightTransform) {
    if (cardboardView == null) return;
    cardboardView.getCurrentEyeParams(headTransform,
                                      leftEye,
                                      rightEye,
                                      monocularEye,
                                      leftEyeNoDistortion,
                                      rightEyeNoDistortion);
    if (headView != null && headView.length >= 16) {
      headTransform.getHeadView(headView, 0);
    }
    if (leftTransform != null && leftTransform.length >= 16) {
      float[] leftView = leftEye.getEyeView();
      System.arraycopy(leftView, 0, leftTransform, 0, 16);
    }
    if (rightTransform != null && rightTransform.length >= 16) {
      float[] rightView = rightEye.getEyeView();
      System.arraycopy(rightView, 0, rightTransform, 0, 16);
    }
  }

  // Reset the head tracker to the current heading, called from input class
  public void ResetHeadTracker() {
    if (cardboardView != null) {
      cardboardView.resetHeadTracker();
    }
  }

  // Undistort and render the provided texture, called from renderer class
  public void UndistortTexture(int textureId) {
    try {
      cardboardView.undistortTexture(textureId);
    } catch (Exception e) {
      Log.e("SDL", "exception", e);
    }
  }

  // Set whether the Cardboard button (gear icon) is enabled and rendering.
  public void SetCardboardButtonEnabled(boolean enabled) {
    if (uiLayer != null) {
      uiLayer.setButtonEnabled(enabled);
    } else if (enabled) {
      uiLayer = new UiLayer(this, GetCardboardButtonDrawable());
      uiLayer.attachUiLayer(null);
    }
  }

  // The drawable used by the Cardboard setting button. null uses the default gear.
  protected Drawable GetCardboardButtonDrawable() {
    return null;
  }

  private OrientationEventListener CreateOrientationListener() {
    return new OrientationEventListener(this) {
        @Override
        public void onOrientationChanged(int orientation) {
          int rotation = getWindowManager().getDefaultDisplay().getRotation();
          if (rotation != cachedDeviceRotation) {
            cachedDeviceRotation = rotation;
            nativeOnDisplayRotationChanged(rotation);
          }
        }
      };
  }

  // Sends a keypress event to the Android system.
  public void SendKeypressEventToAndroid(int androidKeycode) {
    // SendKeyDownUpSync is only allowed (at our current permission level)
    // as long as we only send events to ourselves.  Unfortunately,
    // SendKeyDownUpSync just sends them to whatever has current focus, and
    // key-up events are sent shortly after, with no guarantees that we still
    // have focus by then.  Swapping between activities sometimes causes this
    // to send events to other activities, so while they will bounce harmlessly,
    // we still need to catch the exceptions they generate.
    try {
      if (hasWindowFocus() && changingVolume == false) {
        instrumentation.sendKeyDownUpSync(androidKeycode);
      }
    } catch (SecurityException e) {
      Log.e("SDL", "exception", e);
    }
  }

  public void doFrame(long frameTimeNanos) {
    // Respond to event:
    nativeOnVsync();
    // Renew our callback:
    Choreographer.getInstance().postFrameCallback(this);
  }

  public void relaunch() {
    Context context = getBaseContext();
    PendingIntent intent = PendingIntent.getActivity(
      context, 0,
      getIntent(), Intent.FLAG_ACTIVITY_CLEAR_TOP);
    AlarmManager manager = (AlarmManager)context.getSystemService(Context.ALARM_SERVICE);
    int delay = 1;
    manager.set(AlarmManager.RTC, System.currentTimeMillis() + delay, intent);
    System.exit(2);
  }

  // Implemented in C++. (utilities.cpp)
  private static native void nativeInitVsync();

  // Implemented in C++. (utilities.cpp)
  private static native void nativeCleanupVsync();

  // Implemented in C++. (utilities.cpp)
  private static native void nativeOnVsync();

  // Implemented in C++. (gpg_manager.cpp)
  private static native void nativeOnActivityResult(
      Activity activity,
      int requestCode,
      int resultCode,
      Intent data);

  // Implemented in C++. (input.cpp)
  private static native void nativeOnGamepadInput(
      int controllerId,
      int eventCode,
      int controlCode,
      float x,
      float y);

  // Implemented in C++. (input.cpp)
  private static native void nativeOnCardboardTrigger();

  // Implemented in C++. (input.cpp)
  private static native void nativeSetDeviceInCardboard(boolean inCardboard);

  // Implemented in C++. (input.cpp)
  private static native void nativeOnDisplayRotationChanged(int rotation);

}
