<?php
declare(strict_types=1);
$count = 0;
function verify(bool $ok, string $label): void { global $count; if (!$ok) { throw new RuntimeException($label); } ++$count; }
function fetch_page(string $path, string $agent = 'Mozilla/5.0', string $method = 'GET'): array {
    $context = stream_context_create(['http' => ['method' => $method, 'header' => 'User-Agent: ' . $agent . "\r\n", 'ignore_errors' => true, 'timeout' => 5]]);
    $body = file_get_contents('http://127.0.0.1' . $path, false, $context);
    return [$http_response_header, $body];
}
for ($attempt = 0; $attempt < 30; $attempt++) {
    $socket = @fsockopen('127.0.0.1', 80);
    if ($socket) { fclose($socket); break; }
    usleep(100000);
}
foreach (['', '/PHPVer'] as $base) {
    [$headers, $body] = fetch_page($base . '/');
    verify(str_contains($headers[0], '200') && str_contains($body, 'A stick’s life.'), 'Apache visitor page');
    verify(str_contains($body, 'href="http://127.0.0.1' . $base . '/style.css"'), 'Apache subdirectory assets');
    [$headers, $body] = fetch_page($base . '/?i=1', 'GPTBot');
    verify(str_contains($body, 'Good questions.'), 'Bot and hosting query compatibility');
    verify((bool) preg_grep('/^Vary:.*User-Agent/i', $headers), 'Vary header sent: ' . implode('; ', $headers));
    verify(in_array('Cache-Control: private, no-store', $headers, true), 'Cache header sent');
    foreach (['/library.php', '/library', '/article.php?slug=how-dram-remembers', '/articles/how-dram-remembers'] as $path) {
        [$headers, $body] = fetch_page($base . $path);
        verify(str_contains($headers[0], '200') && str_contains($body, 'How DRAM remembers'), 'Article or library: ' . $path);
    }
    foreach (['/robots.php', '/robots.txt'] as $path) {
        [$headers, $body] = fetch_page($base . $path);
        verify(str_contains($headers[0], '200') && str_contains($body, 'Sitemap:'), 'Robots endpoint');
    }
    foreach (['/manifest.php', '/manifest.webmanifest'] as $path) {
        [$headers, $body] = fetch_page($base . $path);
        verify(str_contains($headers[0], '200') && json_decode($body, true)['name'] === 'ArticleNexus', 'Manifest endpoint');
    }
    foreach (['/sitemap.php', '/sitemap.xml'] as $path) {
        [$headers, $body] = fetch_page($base . $path);
        verify(str_contains($headers[0], '200') && substr_count($body, '<url>') === 8, 'Sitemap endpoint');
    }
    foreach (['/style.css' => ':root', '/client.js' => 'recommend()', '/icon.svg' => '<svg'] as $path => $text) {
        [$headers, $body] = fetch_page($base . $path);
        verify(str_contains($headers[0], '200') && str_contains($body, $text), 'Static asset');
    }
    foreach (['/config.php', '/inc/articles.json', '/inc/app.php', '/inc/human.php', '/inc/'] as $path) {
        [$headers, $body] = fetch_page($base . $path);
        verify(str_contains($headers[0], '403'), 'Protect internal file: ' . $path);
    }
    [$headers, $body] = fetch_page($base . '/article.php?slug[]=bad');
    verify(str_contains($headers[0], '404'), 'Array slug returns 404');
    [$headers, $body] = fetch_page($base . '/article.php?slug=../../config.php');
    verify(str_contains($headers[0], '404'), 'Traversal returns 404');
    [$headers, $body] = fetch_page($base . '/', 'Mozilla', 'HEAD');
    verify(str_contains($headers[0], '200') && $body === '', 'HTTP HEAD');
    [$headers, $body] = fetch_page($base . '/', 'Mozilla', 'POST');
    verify(str_contains($headers[0], '405'), 'HTTP POST');
}
echo "PASS: $count Apache HTTP checks\n";
