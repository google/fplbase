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
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageItemInfo;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.graphics.Point;
import android.hardware.Sensor;
import android.hardware.SensorManager;
import android.media.AudioManager;
import android.os.Bundle;
import android.util.Log;
import android.util.TypedValue;
import android.view.Choreographer;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import org.libsdl.app.SDLActivity;

public class FPLActivity extends SDLActivity implements
    Choreographer.FrameCallback {

  private static final float METERS_PER_INCH = 0.0254f;
  private final Instrumentation instrumentation = new Instrumentation();

  private String getMetaData(String name, PackageItemInfo info) {
    if (info == null || info.metaData == null) return null;
    return info.metaData.getString(name);
  }

  // Looks for 'name' in the <meta-data> tags of AndroidManifest.xml.
  // We look at <meta-data> tags in the relevant <activity> </activity> section
  // first. If 'name' isn't found there, we look at the meta-data in the
  // <application> </application> tags.
  // Returns meta-data value, if found, or null, if not found.
  private String getMetaData(String name) {
    Context context = getBaseContext();

    // Look in our Activity's metadata, first.
    try {
      ActivityInfo activityInfo = context.getPackageManager().getActivityInfo(
        getComponentName(), PackageManager.GET_META_DATA);
      String activityValue = getMetaData(name, activityInfo);
      if (activityValue != null) return activityValue;
    } catch (PackageManager.NameNotFoundException e) {}

    // If not found, then look into the Application's metadata.
    try {
      ApplicationInfo appInfo = context.getPackageManager().getApplicationInfo(
        context.getPackageName(), PackageManager.GET_META_DATA);
      return getMetaData(name, appInfo);
    } catch (PackageManager.NameNotFoundException e) {}

    return null;
  }

  @Override
  protected String[] getLibraries() {
    // Search the metadata for the app name in the same way as NativeActivity.
    // Your AndroidManifest.xml should have a meta-data tag like this:
    // <activity ...>
    //   <meta-data android:name="android.app.lib_name" android:value="mesh" />
    //   ...
    // </activity>
    String app_library_name = getMetaData("android.app.lib_name");
    if (app_library_name == null) {
      Log.i("FPL", "Cannot find android.app.lib_name in AndroidManifest.xml.");
      app_library_name = "main";
    }
    return new String[] { app_library_name };
  }

  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    nativeInitVsync();
  }

  @Override
  public void onDestroy() {
    super.onDestroy();
    nativeCleanupVsync();
  }

  @Override
  public void onResume() {
    super.onResume();
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
    Choreographer.getInstance().removeFrameCallback(this);

    Context context = getBaseContext();
    AudioManager am = (AudioManager)context.getSystemService(Context.AUDIO_SERVICE);
    am.abandonAudioFocus(null);
  }

  // GPG's GUIs need activity lifecycle events to function properly, but
  // they don't have access to them. This code is here to route these events
  // back to GPG through our C++ code.
  @Override
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

    @Override
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

    @Override
    public void run() {
      try {
        queryResponse = -1;
        textDialogOpen = true;

        AlertDialog alert =
            new AlertDialog.Builder(activity, AlertDialog.THEME_DEVICE_DEFAULT_LIGHT)
                .setTitle(title)
                .setMessage(question)
                .setNegativeButton(
                    no,
                    new DialogInterface.OnClickListener() {
                      @Override
                      public void onClick(DialogInterface dialog, int id) {
                        queryResponse = 0;
                        textDialogOpen = false;
                      }
                    })
                .setPositiveButton(
                    yes,
                    new DialogInterface.OnClickListener() {
                      @Override
                      public void onClick(DialogInterface dialog, int id) {
                        queryResponse = 1;
                        textDialogOpen = false;
                      }
                    })
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
      float finalX;
      float finalY;
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

  public int[] GetLandscapedSize() {
    Point size = new Point();
    // Immersive mode uses the full screen, so get the real size if using it
    if (UseImmersiveMode()) {
      getWindowManager().getDefaultDisplay().getRealSize(size);
    } else {
      getWindowManager().getDefaultDisplay().getSize(size);
    }
    return new int[] { Math.max(size.x, size.y), Math.min(size.x, size.y) };
  }

  protected boolean InitializeHeadMountedDisplayOnStart() {
    SensorManager sensorManager = (SensorManager)getSystemService(Context.SENSOR_SERVICE);
    return sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE) != null &&
           sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER) != null;
  }

  // Returns true if the current device is a TV device, false otherwise.
  public boolean IsTvDevice() {
    UiModeManager uiModeManager = (UiModeManager)getSystemService(UI_MODE_SERVICE);
    return uiModeManager.getCurrentModeType() == Configuration.UI_MODE_TYPE_TELEVISION;
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

  @Override
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

}
