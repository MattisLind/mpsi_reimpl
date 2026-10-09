#!/usr/bin/env python3
"""Run Rev2 VHDL tools locally or in the selected Docker image."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
HDL = ROOT / 'hardware/rev2'
TOOLS = HDL / 'rtl/atf15xx-yosys-docker'
IMAGE = os.environ.get('MPSI_HDL_IMAGE', 'atf15xx-ghdl-yosys')


def docker():
    executable = shutil.which('docker')
    if not executable:
        executable = '/Applications/Docker.app/Contents/Resources/bin/docker'
    if not Path(executable).exists():
        raise SystemExit('Docker CLI not found; install Docker or set PATH.')
    env = os.environ.copy()
    env['PATH'] = str(Path(executable).parent) + os.pathsep + env.get('PATH', '')
    return executable, env


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['build-image', 'build-sim-image', 'test', 'synth', 'fit', 'postfit', 'tool-info'])
    parser.add_argument('--fit-options', default='', help='additional Atmel fitter arguments for placement iterations')
    args = parser.parse_args()
    if args.fit_options and args.action not in ('fit', 'postfit'):
        parser.error('--fit-options is only valid with fit/postfit')
    exe, env = docker()
    if args.action == 'build-image':
        subprocess.run([exe, 'build', '--platform', 'linux/amd64', '-t', IMAGE,
                        str(TOOLS)], env=env, check=True)
        return
    if args.action == 'build-sim-image':
        subprocess.run([exe, 'build', '--platform', 'linux/amd64', '-t', 'mpsi-ghdl-sim',
                        '-f', str(HDL / 'tools/Dockerfile.sim'), str(HDL / 'tools')],
                       env=env, check=True)
        return
    image = IMAGE
    if args.action == 'test' and subprocess.run(
            [exe, 'image', 'inspect', image], env=env,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode:
        image = 'mpsi-ghdl-sim'
    command = {'test': ['make', '-C', '/src/hardware/rev2', 'test-local'],
               'synth': ['make', '-C', '/src/hardware/rev2', 'synth-local'],
               'fit': ['make', '-C', '/src/hardware/rev2', 'fit-local'],
               'postfit': ['make', '-C', '/src/hardware/rev2', 'postfit-local'],
               'tool-info': ['bash', '-c', 'ghdl --version; yosys -V; git -C /atf15xx_yosys rev-parse HEAD']}[args.action]
    if args.action in ('fit', 'postfit') and args.fit_options:
        command.append('FIT_OPTIONS=' + args.fit_options)
    try:
        subprocess.run([exe, 'run', '--rm', '--platform', 'linux/amd64',
                        '--mount', f'type=bind,source={ROOT},target=/src',
                        '-e', 'PATH=/oss-cad-suite/bin:/usr/local/bin:/usr/bin:/bin',
                        image, *command], env=env, check=True)
    except subprocess.CalledProcessError as error:
        if args.action in ('fit', 'postfit'):
            folder = ROOT / 'build/rev2/vhdl/fit'
            (folder / 'mpsi_cpld.jed').unlink(missing_ok=True)
            report = folder / 'mpsi_cpld.fit'
            if report.exists():
                for line in report.read_text().splitlines():
                    if 'Aborting' in line or 'JEDEC file not created' in line or '$Device' in line:
                        print(line)
            raise SystemExit(f'{args.action} failed; no programming image accepted. See {folder}/fitter.log and build/rev2/vhdl/postfit')
        raise SystemExit(error.returncode)


if __name__ == '__main__':
    main()
