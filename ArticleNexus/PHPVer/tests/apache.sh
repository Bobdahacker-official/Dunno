#!/bin/sh
set -eu
cp -R /app/public/. /var/www/html/
mkdir -p /var/www/html/PHPVer
cp -R /app/public/. /var/www/html/PHPVer/
a2enmod rewrite >/dev/null
cat > /etc/apache2/conf-enabled/articlenexus-test.conf <<'CONFIG'
ServerName localhost
<Directory /var/www/html>
    AllowOverride All
</Directory>
CONFIG
apache2-foreground >/tmp/article-apache.log 2>&1 &
apache_pid=$!
trap 'kill "$apache_pid" 2>/dev/null || true' EXIT
php /app/tests/http.php
