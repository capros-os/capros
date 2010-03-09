# Translate base64 characters into characters legal in URLs.
s/=//g
s/+/-/g
s./._.g
