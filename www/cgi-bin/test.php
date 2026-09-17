<?php
$method  = $_SERVER['REQUEST_METHOD'] ?? 'GET';
$query   = htmlspecialchars($_SERVER['QUERY_STRING'] ?? '');
$session = isset($_COOKIE['session_id']) ? $_COOKIE['session_id'] : '';
$name    = isset($_GET['name']) ? htmlspecialchars($_GET['name']) : 'World';
?>
Content-Type: text/html

<!DOCTYPE html>
<html>
<head><title>PHP CGI Test</title></head>
<body>
<h1>Hello, <?= $name ?>!</h1>
<p>Method: <?= $method ?></p>
<p>Query string: <?= $query ?></p>
<p>Session ID: <?= htmlspecialchars($session) ?></p>
<form method="get">
  Name: <input name="name" value="<?= $name ?>">
  <button type="submit">Greet</button>
</form>
</body>
</html>
