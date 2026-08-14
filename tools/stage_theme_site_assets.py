#!/usr/bin/env python3
"""Stage browser and catalog assets beside generated SwitchU theme packages.

The console downloads only `theme.zip`.  The public catalog also needs an
unpacked manifest, a still cover and lightweight browser previews, none of
which should be duplicated inside the Switch download flow.
"""
import argparse
import os
import shutil
import subprocess
import sys
import zipfile

from PIL import Image

from build_bc7_theme_catalog import discover_videos


def run_ffmpeg(ffmpeg, arguments):
    subprocess.run([ffmpeg, '-y', '-loglevel', 'error', *arguments], check=True)


def stage_package(package, video, destination, ffmpeg):
    os.makedirs(destination, exist_ok=True)
    shutil.copyfile(package, os.path.join(destination, 'theme.zip'))
    with zipfile.ZipFile(package) as archive:
        manifest = archive.read('theme.json')
        cover = archive.read('media/screenshots/00.jpg')
    with open(os.path.join(destination, 'theme.json'), 'wb') as output:
        output.write(manifest)
    with open(os.path.join(destination, 'media-screenshot.jpg'), 'wb') as output:
        output.write(cover)

    # Keep manifest paths valid on the static site.  The console has this same
    # image inside theme.zip after installation.
    screenshot_dir = os.path.join(destination, 'media', 'screenshots')
    os.makedirs(screenshot_dir, exist_ok=True)
    shutil.copyfile(os.path.join(destination, 'media-screenshot.jpg'),
                    os.path.join(screenshot_dir, '00.jpg'))
    os.remove(os.path.join(destination, 'media-screenshot.jpg'))

    # A preview is presentation-only.  Preserve its source stream whenever it
    # is already browser-native: avoiding a second lossy encode is especially
    # important for high-contrast animated artwork, where H.264 MF can produce
    # temporal noise around neon edges.  All supplied WebM files are VP9 and
    # play natively in the browsers supported by the catalog.
    source_extension = os.path.splitext(video)[1].lower()
    if source_extension == '.webm':
        shutil.copyfile(video, os.path.join(destination, 'preview.webm'))
    else:
        run_ffmpeg(ffmpeg, ['-i', video, '-an', '-map_metadata', '-1',
                            '-vf', 'scale=1920:-2:flags=lanczos',
                            '-c:v', 'h264_mf', '-pix_fmt', 'yuv420p',
                            '-movflags', '+faststart',
                            os.path.join(destination, 'preview_1080.mp4')])
    run_ffmpeg(ffmpeg, ['-i', video, '-frames:v', '1', '-q:v', '3',
                        os.path.join(destination, 'preview_1080.jpg')])

    frames_dir = os.path.join(destination, '.preview-frames')
    os.makedirs(frames_dir, exist_ok=True)
    try:
        run_ffmpeg(ffmpeg, ['-i', video, '-vf', 'fps=8,scale=228:128:flags=lanczos',
                            '-frames:v', '20', os.path.join(frames_dir, 'f%02d.jpg')])
        frames = sorted(os.path.join(frames_dir, name) for name in os.listdir(frames_dir)
                        if name.lower().endswith('.jpg'))
        if not frames:
            raise RuntimeError('ffmpeg produced no preview frames')
        while len(frames) < 20:
            frames.append(frames[-1])
        sheet = Image.new('RGB', (228 * 5, 128 * 4))
        for index, path in enumerate(frames[:20]):
            with Image.open(path) as frame:
                sheet.paste(frame.convert('RGB'), ((index % 5) * 228, (index // 5) * 128))
        sheet.save(os.path.join(destination, 'preview_sheet.jpg'), quality=84, optimize=True)
    finally:
        shutil.rmtree(frames_dir, ignore_errors=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--packages', required=True)
    parser.add_argument('--videos', nargs='+', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--ffmpeg', default='ffmpeg')
    args = parser.parse_args()

    videos = discover_videos(args.videos)
    packages = sorted(name for name in os.listdir(args.packages) if name.endswith('.zip'))
    staged = 0
    for name in packages:
        ident = name[:-4]
        video = videos.get(ident)
        if not video:
            continue
        print('staging ' + ident, flush=True)
        stage_package(os.path.join(args.packages, name), video,
                      os.path.join(args.output, ident), args.ffmpeg)
        staged += 1
    print('staged %d themes' % staged)


if __name__ == '__main__':
    main()
