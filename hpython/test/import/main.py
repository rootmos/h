import base64

print(str(base64.b64encode("foo".encode("UTF-8")), "UTF-8"))
