#! /usr/bin/python3
import base64
import argparse


def encode_to_base64(data: str) -> bytes:
    data_bytes = data.encode("utf-8")
    return base64.b64encode(data_bytes)


def decode_from_base64(data: str) -> bytes:
    data += '=' * ((4 - len(data) % 4) % 4)
    data_bytes = data.encode("utf-8")
    return base64.b64decode(data_bytes)


def main():
    parser = argparse.ArgumentParser(description='Base 64 encode/decode util')
    parser.add_argument('-e', '--encode', metavar='decodedData', help='encode data to base64')
    parser.add_argument('-d', '--decode', metavar='encodedData', help='decode data from base64')
    args = vars(parser.parse_args())
    encode_val = args['encode']
    decode_val = args['decode']

    if encode_val is None and decode_val is None:
        parser.print_help()
        exit(1)

    if encode_val is not None:
        print(str(encode_to_base64(encode_val)))

    if decode_val is not None:
        print(str(decode_from_base64(decode_val)))
    exit(0)


if __name__ == '__main__':
    main()
