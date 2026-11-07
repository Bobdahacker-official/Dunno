<?php
declare(strict_types=1);
define('ARTICLE_NEXUS', true);
require dirname(__DIR__) . '/public/inc/app.php';
$count = 0;
function check(bool $condition, string $message): void {
    global $count;
    if (!$condition) { throw new RuntimeException($message); }
    ++$count;
}
$server = ['HTTP_HOST' => 'example.com', 'HTTPS' => 'on', 'SCRIPT_NAME' => '/index.php', 'REQUEST_METHOD' => 'GET', 'HTTP_USER_AGENT' => 'Mozilla/5.0'];
foreach (['', '/PHPVer', '/nested/demo'] as $directory) {
    $env = array_merge($server, ['SCRIPT_NAME' => $directory . '/index.php']);
    $base = 'https://example.com' . $directory;
    [$status, $headers, $html] = respond('home', $env, [], []);
    check($status === 200, 'Visitor status');
    check(str_contains($html, 'A stick’s life.'), 'Visitor storyboard');
    check(str_contains($html, '<title>ArticleNexus</title>'), 'SEO title');
    check(str_contains($html, 'href="' . $base . '/style.css"'), 'CSS path');
    check(str_contains($html, 'src="' . $base . '/client.js"'), 'JS path');
    check(str_contains($html, 'href="' . $base . '/manifest.php"'), 'Manifest path');
    check(substr_count($html, 'CHAPTER 0') === 3, 'Three chapters');
    foreach (['GPTBot', 'ChatGPT-User', 'OAI-SearchBot', 'ClaudeBot', 'Claude-User', 'PerplexityBot', 'Perplexity-User', 'Googlebot', 'bingbot', 'Applebot'] as $agent) {
        [$status, $headers, $html] = respond('home', array_merge($env, ['HTTP_USER_AGENT' => $agent]), [], []);
        check($status === 200 && str_contains($html, 'Good questions.'), 'Agent library: ' . $agent);
        check(!str_contains($html, 'RAM APPRECIATION SOCIETY'), 'Distinct content');
        check(substr_count($html, 'class="article-card"') === 6, 'All six articles');
        check($headers['Vary'] === 'User-Agent' && $headers['Cache-Control'] === 'private, no-store', 'Cache isolation');
        check(str_contains($html, 'rel="canonical" href="' . $base . '/"'), 'Home canonical');
    }
    [$status, $headers, $html] = respond('library', $env, [], []);
    check($status === 200 && str_contains($html, 'Good questions.'), 'Humans can browse library');
    check(str_contains($html, $base . '/article.php?slug=how-dram-remembers'), 'Article links include subdirectory');
    $articles = json_decode(file_get_contents(dirname(__DIR__) . '/public/inc/articles.json'), true, 512, JSON_THROW_ON_ERROR);
    foreach ($articles as $article) {
        [$status, $headers, $html] = respond('article', $env, ['slug' => $article['slug']], []);
        check($status === 200 && str_contains($html, h($article['title'])), 'Article title');
        foreach ($article['sections'] as [$heading, $body]) {
            check(str_contains($html, h($heading)) && str_contains($html, h($body)), 'Complete article section');
        }
        foreach ($article['sources'] as [$title, $link]) { check(str_contains($html, h($link)), 'Source link'); }
        check(str_contains($html, 'application/ld+json'), 'Article schema');
    }
    [$status, $headers, $body] = respond('manifest', $env, [], []);
    $manifest = json_decode($body, true, 512, JSON_THROW_ON_ERROR);
    check($manifest['start_url'] === $base . '/' && $manifest['scope'] === $base . '/', 'Manifest scope');
    check($headers['Content-Type'] === 'application/manifest+json; charset=utf-8', 'Manifest MIME');
    [$status, $headers, $xml] = respond('sitemap', $env, [], []);
    check(substr_count($xml, '<url>') === 8, 'Sitemap includes all pages');
    check(str_contains($xml, $base . '/article.php?slug=what-dns-does'), 'Sitemap article URLs');
    [$status, $headers, $txt] = respond('robots', $env, [], []);
    check(str_contains($txt, 'Sitemap: ' . $base . '/sitemap.php'), 'Robots sitemap');
}
foreach (['not-a-real-slug', '../../config.php', '<script>alert(1)</script>', ['bad']] as $slug) {
    [$status, $headers, $html] = respond('article', $server, ['slug' => $slug], []);
    check($status === 404 && $headers['X-Robots-Tag'] === 'noindex', 'Invalid slugs safely return 404');
    check(!str_contains($html, '<script>alert(1)</script>'), 'No reflected script injection');
}
foreach (['home', 'library', 'article', 'manifest', 'robots', 'sitemap'] as $route) {
    [$status, $headers, $body] = respond($route, array_merge($server, ['REQUEST_METHOD' => 'HEAD']), ['slug' => 'what-dns-does'], []);
    check($status === 200 && $body === '', 'HEAD response');
    [$status, $headers] = respond($route, array_merge($server, ['REQUEST_METHOD' => 'POST']), [], []);
    check($status === 405 && $headers['Allow'] === 'GET, HEAD', 'Unsupported method');
}
check(!article_agent('') && !article_agent('OrdinaryBrowser'), 'Default to visitor');
check(public_base($server, 'https://canonical.test/subfolder/') === 'https://canonical.test/subfolder', 'Explicit base URL');
check(public_base(array_merge($server, ['HTTP_HOST' => '"><script>']), '') === 'https://localhost', 'Reject invalid Host');
check(public_base(array_merge($server, ['HTTPS' => 'off', 'HTTP_X_FORWARDED_PROTO' => 'https']), '') === 'http://example.com', 'Do not trust forwarded headers');
try { public_base($server, 'javascript:alert(1)'); check(false, 'Reject invalid configuration'); } catch (RuntimeException $e) { check(true, 'Reject invalid configuration'); }
echo "PASS: $count PHP checks\n";
