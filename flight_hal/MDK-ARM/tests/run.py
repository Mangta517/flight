"""Compile real firmware modules with host HAL/RTOS fixtures; never access hardware."""
from pathlib import Path
import shutil
import subprocess
import tempfile

tests = Path(__file__).resolve().parent
root = tests.parent
repo = root.parents[1]
gcc = shutil.which('gcc')
if not gcc:
    gcc = r'C:\msys64\mingw64\bin\gcc.exe'

with tempfile.TemporaryDirectory(prefix='flight-host-') as directory:
    build = Path(directory)

    def compile_test(name, sources, interface=None):
        if interface is None:
            interface = root / 'interface'
        output = build / (name + '.exe')
        subprocess.run([gcc, '-std=c99', '-Wall', '-Wextra', '-Wno-unused-parameter',
                        '-I' + str(tests / 'host'), '-I' + str(root / 'application'),
                        '-I' + str(root / 'common'), '-I' + str(interface),
                        *map(str, sources), '-lm', '-o', str(output)], check=True)
        return output

    application = compile_test('flight', [tests / 'test_application_flight.c',
        root / 'application/application_flight.c', root / 'application/application_recevie_data.c',
        root / 'common/common_pid.c'])
    for case in ['init_failure', 'calibration', 'calibration_failure', 'ramp', 'wrap',
                 'freshness', 'read_failure', 'read_deadline', 'snapshot', 'validation', 'idle_trim']:
        subprocess.run([str(application), case], check=True)
    mpu = compile_test('mpu', [tests / 'test_mpu6050.c', root / 'interface/my_mpu6050.c'])
    subprocess.run([str(mpu)], check=True)
    for project in ['flight_hal', 'flight_remote_hal']:
        interface = repo / project / 'MDK-ARM/interface'
        radio = compile_test(project + '-radio', [tests / 'test_radio.c', interface / 'nRF24L01P.c'], interface)
        subprocess.run([str(radio)], check=True)
    pid = compile_test('pid', [tests / 'test_common_pid.c', tests / 'unity.c', root / 'common/common_pid.c'])
    subprocess.run([str(pid)], check=True)
