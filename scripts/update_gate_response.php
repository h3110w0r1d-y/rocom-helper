<?php
declare(strict_types=1);

// 需要 PHP GMP 扩展。
// 这里用 RSA 私钥运算 + PKCS#1 v1.5 type 2 随机 padding。
// C++ 端用 n/e 做公钥还原，去 padding 后比对 EXPECTED_BYTES。

$RSA_E = '65537';
$RSA_P_HEX = 'ecf06a643d143e7c1df13c1ceef28c43b6ae7ea4f08235c78aa5906ee4c5085a95eac1a34028b14b03db7bffb42178500448ff81a1ffa64c238d5cf2958d4e715dbddec754a2ab42fd3f23c294478536a2e01a4541db1cae852824d553085afe3ee80659377dac446465a381057bf43eaa68af505a0bd0070b13190d38b8bba59172f9e6e714665a0010f001eb36b939f01c978d57faf4fd450112ecb3052bad3a736147efc99d40831e832a93053a096cef524c54533fb55eb233ec173949959d9dc60c2d8c70ac4e8350c255ff8c5f8e8f98d77796b6d6707f423557f5d732ac6606070e2094d9766187488c206376a8d46a415ec97502b4443454a3c3b757';
$RSA_Q_HEX = 'd6186ad2a326836c1dd458bdf864c0a7d4a0be44258ce8aaed4031b8e3cc3ab91e0f5505392049fa356438a5bffb0f5470c270697b171dfd893274e20e8fb3981a0ec16790e332cabaa4b2e0c4ffac729a74f066a3e845ba855bbdf315096f9085fd00ce04593b5323678a31bb64a9b65cdac5019022049f110651b18c17bc7ab3f80ad1150b020d791e2dfa98cc2ecd28cbb4e0a166d06286982d8d366c1cc4a8bb4e4b093130f30a3a318dbe34964c54a8f8f18948151778fb2dfffe77edb695cb8bfa1baab99e2ca5120e3b1d23363115c917b4881cfcd24c6210d8f3c5ea433b96f3948099af849c8be656ea79ebe8b8404e2fe6904b134f9a332a6f6827';

$EXPECTED_BYTES = "\x60\x66\x25\x55\x28\xd1\x45\x77\x94\x7f\x5f\xdb\x75\x9f\x93\x15\x91\x64\x9b\x34\xf5\xec\xae\xa5\x61\xaf\x45\x2b\x60\x58\x07\x84";


function gmp_from_bytes(string $bytes): GMP
{
    $hex = bin2hex($bytes);
    return gmp_init($hex === '' ? '0' : $hex, 16);
}

function gmp_to_bytes(GMP $value, int $size): string
{
    $hex = gmp_strval($value, 16);
    if ((strlen($hex) % 2) !== 0) {
        $hex = '0' . $hex;
    }
    $bytes = hex2bin($hex);
    if ($bytes === false) {
        $bytes = '';
    }
    if (strlen($bytes) > $size) {
        return substr($bytes, -$size);
    }
    return str_pad($bytes, $size, "\x00", STR_PAD_LEFT);
}

function non_zero_random_bytes(int $length): string
{
    $result = '';
    while (strlen($result) < $length) {
        $chunk = random_bytes($length - strlen($result));
        $chunk = str_replace("\x00", '', $chunk);
        $result .= $chunk;
    }
    return $result;
}

function rsa_private_encrypt_pkcs1_v15(string $payload, string $pHex, string $qHex, string $eText): string
{
    $p = gmp_init($pHex, 16);
    $q = gmp_init($qHex, 16);
    $e = gmp_init($eText, 10);
    $n = gmp_mul($p, $q);
    $phi = gmp_mul(gmp_sub($p, 1), gmp_sub($q, 1));
    $d = gmp_invert($e, $phi);
    if ($d === false) {
        throw new RuntimeException('Invalid RSA parameters: e has no inverse.');
    }

    $modulusSize = intdiv(strlen(gmp_strval($n, 16)) + 1, 2);
    $maxPayloadSize = $modulusSize - 11;
    if (strlen($payload) > $maxPayloadSize) {
        throw new RuntimeException("Payload too large, max bytes: {$maxPayloadSize}");
    }

    $paddingSize = $modulusSize - strlen($payload) - 3;
    $encoded = "\x00\x02" . non_zero_random_bytes($paddingSize) . "\x00" . $payload;
    $cipher = gmp_powm(gmp_from_bytes($encoded), $d, $n);
    return gmp_to_bytes($cipher, $modulusSize);
}

try {
    header('Content-Type: text/plain; charset=utf-8');
    echo base64_encode(rsa_private_encrypt_pkcs1_v15($EXPECTED_BYTES, $RSA_P_HEX, $RSA_Q_HEX, $RSA_E));
} catch (Throwable $e) {
    http_response_code(500);
    echo '检查更新失败，软件退出';
}
