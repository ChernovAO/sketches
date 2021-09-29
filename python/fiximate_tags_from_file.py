#! /usr/bin/python3

import re
import typing

from requests_html import HTMLSession


def process_links(link: str, log_file: typing.TextIO):
    link = link.strip()
    print("link to check: " + link)
    if not re.match("https://fiximate.fixtrading.org/en/FIX.Latest/tag\\d+.html", link):
        print("Error: link " + link + " is not tag link")
        return
    session = HTMLSession()
    r = session.get(link)
    r.html.render(timeout=20)
    log_file.write(link + ":\n")
    log_file.write(r.html.html)
    log_file.write("\n\n\n")

def main():
    with open("log.txt", "w+") as log_file:
        with open("result.txt", "r") as links_file:
            for link in links_file:
                process_links(link, log_file)
    exit(0)


if __name__ == '__main__':
    main()
