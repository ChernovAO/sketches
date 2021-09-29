#! /usr/bin/python3
import typing

from requests_html import HTMLSession


def process_tag_link(log_file: typing.TextIO, link):
    print("Start processing tag link:" + link)
    log_file.write(link)
    log_file.write("\n")


def main():
    print("Processing was started")
    tags_by_number = "https://fiximate.fixtrading.org/en/FIX.Latest/fields_sorted_by_tagnum.html"
    session = HTMLSession()
    r = session.get(tags_by_number)
    r.html.render(timeout=20)
    with open("result.txt", "w") as log_file:
        # f.write(r.html.html)
        for link in r.html.absolute_links:
            print(link)
            log_file.write(link)
            log_file.write("\n")
        log_file.close()
    print("Processing was finished")
    exit(0)


if __name__ == '__main__':
    main()
