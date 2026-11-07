<?php
declare(strict_types=1);
define('ARTICLE_NEXUS', true);
require __DIR__ . '/inc/app.php';
$config = require __DIR__ . '/config.php';
try {
    [$status, $headers, $body] = respond($route ?? 'home', $_SERVER, $_GET, $config);
    http_response_code($status);
    foreach ($headers as $name => $value) { header($name . ': ' . $value); }
    echo $body;
} catch (Throwable $error) {
    error_log('ArticleNexus: ' . $error->getMessage());
    http_response_code(500);
    header('Content-Type: text/plain; charset=utf-8');
    header('Cache-Control: no-store');
    echo 'Unable to load this page.';
}
