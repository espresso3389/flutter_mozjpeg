import 'dart:io';

import 'package:flutter/material.dart';
import 'dart:async';
import 'dart:ui' as ui;

import 'package:flutter_mozjpeg/flutter_mozjpeg.dart';
import 'package:image_picker/image_picker.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatefulWidget {
  const MyApp({Key? key}) : super(key: key);

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  _ConversionResult? result;
  ui.Image? originalImage;
  final textEditingController = TextEditingController();
  final progress = ValueNotifier<List<int>>([0, 0, 0]); // pass, totalPass, percentage

  @override
  void initState() {
    super.initState();
  }

  @override
  void dispose() {
    textEditingController.dispose();
    progress.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(
          title: const Text('Plugin example app'),
        ),
        body: Column(
          children: [
            InteractiveViewer(
              child: Stack(
                children: [
                  RawImage(image: originalImage),
                  RawImage(image: result?.image),
                ],
              ),
            ),
            ValueListenableBuilder<List<int>>(
                valueListenable: progress,
                builder: (context, value, child) {
                  return Column(
                    children: [
                      result == null
                          ? Text(
                              'Pass ${value[0].toString().padLeft(3)}/${value[1].toString().padLeft(3)}: ${value[2].toString().padLeft(3)}%',
                              style: const TextStyle(fontFamily: 'courier'),
                            )
                          : Text(
                              'Size: ${result!.inputSize} -> ${result!.outputSize} (${((result!.outputSize / result!.inputSize) * 100).toStringAsFixed(1)}%)',
                              style: const TextStyle(fontFamily: 'courier'),
                            ),
                      LinearProgressIndicator(
                        value: value[2] / 100,
                        minHeight: 5,
                      ),
                    ],
                  );
                }),
            Expanded(
              child: TextField(
                controller: textEditingController,
                keyboardType: TextInputType.multiline,
                maxLines: null,
                readOnly: true,
                expands: true,
                style: const TextStyle(fontFamily: 'courier'),
              ),
            ),
          ],
        ),
        floatingActionButton: FloatingActionButton(
          tooltip: 'Pick an Image',
          onPressed: _compressImage,
          child: const Icon(Icons.image),
        ),
      ),
    );
  }

  Future<void> _compressImage() async {
    final imageFile = await ImagePicker().pickImage(source: ImageSource.gallery, maxWidth: 2000, maxHeight: 2000);
    if (imageFile == null) return;

    result?.dispose();
    result = null;
    originalImage = await loadImage(File(imageFile.path));
    if (mounted) {
      setState(() {});
    }

    if (originalImage == null) {
      return;
    }

    textEditingController.text = '';
    FlutterMozjpeg.messageCallback = (m) {
      if (!mounted) return;
      textEditingController.text += m;
    };

    final convResult = await FlutterMozjpeg.jpegCompressImage(
      originalImage!,
      progressCallback: (pass, totalPass, percentage) {
        progress.value = [pass, totalPass, percentage];
      },
    );

    if (convResult != null) {
      final prev = result;
      result = _ConversionResult(
          image: await convResult.createImage(),
          inputSize: await imageFile.length(),
          outputSize: convResult.buffer.lengthInBytes);
      convResult.dispose();
      prev?.dispose();
      if (mounted) {
        setState(() {});
      }
    }
  }
}

Future<ui.Image> loadImage(File file) async {
  final comp = Completer<ui.Image>();
  ui.decodeImageFromList(await file.readAsBytes(), (r) => comp.complete(r));
  return await comp.future;
}

class _ConversionResult {
  _ConversionResult({required this.image, required this.inputSize, required this.outputSize});
  final ui.Image image;
  final int inputSize;
  final int outputSize;

  void dispose() {
    image.dispose();
  }
}
