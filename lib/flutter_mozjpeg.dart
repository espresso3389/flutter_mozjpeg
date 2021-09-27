// You have generated a new plugin project without
// specifying the `--platforms` flag. A plugin project supports no platforms is generated.
// To add platforms, run `flutter create -t plugin --platforms <platforms> .` under the same
// directory. You can also find a detailed instruction on how to add platforms in the `pubspec.yaml` at https://flutter.dev/docs/development/packages-and-plugins/developing-packages#plugin-platforms.

import 'dart:ffi';
import 'dart:io';
import 'dart:isolate';

import 'package:ffi/ffi.dart';

typedef Dart_InitializeApiDLFunc = Pointer<Void> Function(Pointer<Void>);

typedef MessageCallback = void Function(String);
typedef ProgressCallback = void Function(int pass, int totalPass, int percentage);

typedef _MainTypeFunc = int Function(int argc, Pointer<Pointer<Utf8>> argv, int context);
typedef _SetDartPortFunc = void Function(int port);

class FlutterMozjpeg {
  static final mozJpegLib =
      Platform.isAndroid ? DynamicLibrary.open("libflutter_mozjpeg.so") : DynamicLibrary.process();

  /// [How to use async callback between C++ and Dart with FFI?](https://github.com/flutter/flutter/issues/63255)
  /// - Copy dart-sdk's header/impl. files and call `Dart_InitializeApiDL` with [NativeApi.initializeApiDLData](https://api.flutter.dev/flutter/dart-ffi/NativeApi/initializeApiDLData.html).
  static final Dart_InitializeApiDLFunc _Dart_InitializeApiDL =
      mozJpegLib.lookup<NativeFunction<Dart_InitializeApiDLFunc>>("Dart_InitializeApiDL").asFunction();
  static final _SetDartPortFunc _setDartPort =
      mozJpegLib.lookup<NativeFunction<Void Function(Int64)>>("set_dart_port").asFunction();
  static Pointer<Void>? cookie;
  static void _ensureDartApiInitialized() {
    cookie ??= _Dart_InitializeApiDL(NativeApi.initializeApiDLData);
    final pub = ReceivePort()
      ..listen((message) {
        if (message is String) {
          messageCallback?.call(message);
          return;
        }
        if (message is List && message.length == 4) {
          _progressCallbacks[message[0] as int]?.call(message[1] as int, message[2] as int, message[3] as int);
        }
      });
    _setDartPort(pub.sendPort.nativePort);
  }

  static MessageCallback? messageCallback;

  static final _progressCallbacks = <int, ProgressCallback?>{};
  static int _pcnIndex = 0;

  static int _addProgressCallback(ProgressCallback? progressCallback) {
    final int context = ++_pcnIndex;
    _progressCallbacks[context] = progressCallback;
    return context;
  }

  static void _removeProgressCallback(int context) {
    _progressCallbacks.remove(context);
  }

  static final _MainTypeFunc _jpegtran =
      mozJpegLib.lookup<NativeFunction<Int32 Function(Int32, Pointer<Pointer<Utf8>>, IntPtr)>>("jpegtran").asFunction();

  static int _callMain(
    _MainTypeFunc func,
    List<String> args,
    String exeName,
    int context,
  ) {
    _ensureDartApiInitialized();

    final argv = calloc.allocate<Pointer<Utf8>>((args.length + 2) * sizeOf<Pointer<Utf8>>());
    try {
      argv[0] = exeName.toNativeUtf8();
      for (int i = 0; i < args.length; i++) {
        argv[i + 1] = args[i].toNativeUtf8();
      }

      return func(
        args.length + 1,
        argv,
        context,
      );
    } finally {
      for (int i = 0; i <= args.length; i++) {
        malloc.free(argv[i]);
      }
      calloc.free(argv);
    }
  }

  static int jpegtran(
    List<String> args, {
    ProgressCallback? progressCallback,
  }) {
    final context = _addProgressCallback(progressCallback);
    try {
      return _callMain(_jpegtran, args, "jpegtran", context);
    } finally {
      _removeProgressCallback(context);
    }
  }
}
