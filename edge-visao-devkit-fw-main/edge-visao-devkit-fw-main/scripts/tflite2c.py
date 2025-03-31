import argparse
import binascii
import pathlib

import jinja2

from tensorflow.lite.python.interpreter import Interpreter
from tensorflow.lite.tools.visualize import CreateDictFromFlatbuffer, OpCodeMapper, BuiltinCodeToName

SUPPORTED_MODEL_FORMATS = ('.tflite',)


def camelcase(s):
    tokens = s.split('_')
    return ''.join(map(lambda t: t.capitalize(), tokens)).replace('2d', '2D')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('model_path', type=str)
    parser.add_argument('output_path', type=str)

    args = parser.parse_args()
    model_path = pathlib.Path(args.model_path)
    output_path = pathlib.Path(args.output_path)

    model_name, extension = model_path.stem, model_path.suffix

    if extension not in SUPPORTED_MODEL_FORMATS:
        raise ValueError(f"Extension '{extension}' not supported")

    with open(model_path, 'rb') as f:
        model_bytes = binascii.b2a_hex(f.read()).decode('utf-8').upper()
        model_hex = [f'0x{model_bytes[i:i + 2]}' for i in range(0, len(model_bytes), 2)]

    model_array = ',\n\t\t'.join([', '.join(model_hex[i:i + 10]) for i in range(0, len(model_hex), 10)])

    tflite_model = Interpreter(str(model_path))

    input_details = tflite_model.get_input_details()
    output_details = tflite_model.get_output_details()

    with open(str(model_path), 'rb') as file_handle:
        file_data = bytearray(file_handle.read())
    data = CreateDictFromFlatbuffer(file_data)
    op_codes = list(
        map(BuiltinCodeToName, [operator_code['deprecated_builtin_code'] for operator_code in data['operator_codes']]))

    loader = jinja2.FileSystemLoader(pathlib.Path('templates').absolute())
    env = jinja2.Environment(loader=loader)
    env.filters['camelcase'] = camelcase
    context = {
        'model_name': model_name,
        'model_len': len(model_hex),
        'model_array': model_array,
        'input_details': input_details,
        'output_details': output_details,
        'op_codes_len': len(op_codes),
        'op_codes': op_codes,
    }

    header = env.get_template('model_header.j2')
    with open(output_path / f'{model_name}.h', 'w') as f:
        f.write(header.render(**context))

    source = env.get_template('model_source.j2')
    with open(output_path / f'{model_name}.cpp', 'w') as f:
        f.write(source.render(**context))
