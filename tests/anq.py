#!/usr/bin/python3

import sys, json
from http.server import HTTPServer,BaseHTTPRequestHandler

# ~ Sample
# ~ http://anqtestapi.nms.com.ng/api/json/LookUpNumber/GsmPortStatus?username=xxxxxx@anq.com&password=xxcf
# ~ c&ServiceType=4&numbersToLookUp=08075597646&country=234

class handler(BaseHTTPRequestHandler):
    def do_GET(self):
        print(self.path)

        (path, _) = self.path.split('?')
        print(path)
        if path != '/api/json/LookUpNumber/GsmPortStatus':
            self.send_error(404)
            return

        message = json.dumps({ "Result": [ {
            "Number": "2347068970633",
            "CountryCode": "235",
            "TheOperator": "ETS",
            "IsPorted": 1,
            "OperatorMobileNumberCode": "49",
            "UniversalNumberFormat": "2347068970633"
        }]}, sort_keys=True, indent=4) + "\n"
        self.send_response(200)
        self.end_headers()
        self.wfile.write(message.encode('utf-8'))

server_address = ('', 8000)
httpd = HTTPServer(('', 8000), handler)
httpd.serve_forever()
