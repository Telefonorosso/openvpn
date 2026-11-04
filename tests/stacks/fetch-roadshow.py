#!/usr/bin/env python3
"""Download the official publisher-linked Roadshow demo, checking the pinned bytes."""
import argparse
import hashlib
import html
from html.parser import HTMLParser
import http.cookiejar
import json
from pathlib import Path
import urllib.parse
import urllib.request


class DownloadForm(HTMLParser):
    def __init__(self):
        super().__init__()
        self.action = None
        self.fields = {}
        self.active = False

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == 'form':
            self.active = attrs.get('name') == 'download'
            if self.active:
                self.action = attrs.get('action')
        elif self.active and tag == 'input' and 'name' in attrs:
            self.fields[attrs['name']] = attrs.get('value', '')

    def handle_endtag(self, tag):
        if tag == 'form':
            self.active = False


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, default=Path('build/stack-dependencies'))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    pin = json.loads((root / 'ci/stack-dependencies.json').read_text())['roadshow']
    args.cache.mkdir(parents=True, exist_ok=True)
    archive = args.cache / pin['archive']
    if not archive.exists():
        opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(http.cookiejar.CookieJar()))
        with opener.open(pin['url'], timeout=30) as response:
            page = response.read(2 * 1024 * 1024).decode()
        form = DownloadForm()
        form.feed(page)
        if not form.action or form.fields.get('df_id') != '3657':
            raise SystemExit('Publisher download form changed; review it instead of bypassing checks')
        action = urllib.parse.urljoin(pin['url'], html.unescape(form.action))
        if urllib.parse.urlsplit(action).netloc != urllib.parse.urlsplit(pin['url']).netloc:
            raise SystemExit('Refusing unexpected download form destination')
        request = urllib.request.Request(action, urllib.parse.urlencode(form.fields).encode())
        with opener.open(request, timeout=60) as response:
            data = response.read(4 * 1024 * 1024)
        if hashlib.sha256(data).hexdigest() != pin['sha256']:
            raise SystemExit('Roadshow demo SHA256 mismatch; no test input accepted')
        archive.write_bytes(data)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != pin['sha256']:
        raise SystemExit('Cached Roadshow demo SHA256 mismatch')
    print(archive.resolve())


if __name__ == '__main__':
    main()
