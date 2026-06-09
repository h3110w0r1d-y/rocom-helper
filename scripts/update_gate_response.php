<?php
declare(strict_types=1);

// 需要 PHP GMP 扩展。
// 这里用 RSA 私钥运算 + PKCS#1 v1.5 type 2 随机 padding。
// C++ 端用 n/e 做公钥还原，去 padding 后比对 EXPECTED_BYTES_HEX 对应的原始字节。

$RSA_E = '65535';
$RSA_P_HEX = 'f6a8e603969de09b5c48e043c28f7983f3ee67c9f4908fbc9dbe6620ca7fd0faf59bf44224eff9b37aae060af8e70f38df4a741b423ee295435d36165e4bb8c86d3abfba5bfd517ee0bd6774c0d923f48302cc30d0058b5f4a455984205735b6e68bf258b4813a87cd7f5037b0bbfc650187ebd23144ccbd73668d2dd591887f9655d1141e1a34ad3236e2e967fa098181cba7779c51da5ee5c9295f3004adf1c29565d5b5a7654dc2d4e30cac9c059a3766ebde119112963023d300292fd9425b8fa66918b534344347c54d59fcd4a45d462fd6cb26cb4ce4e338429087cbeaa8d5907571edfd5093d66f0a782a4cfed49ec9e227ca9585a8f017c13b24a147';
$RSA_Q_HEX = '9589b0d089c5491d8766d21c0cd175f7911982f26a87248924b9a5273546d177f339812d9c342018856c7050f225f612880ec325e57266f40891e612a3ce2137dc631aae6a4412f9fea908f4ebe54a18803eb5551bfb9548f9ecfa91d73741530a996040d4bbcb949006e653214e3128fec42bc475d5e801b44d5c8c9352909bd96e878aeac5a798290e12afcc388884e739a391e0b387ade1733d37566b89669db8591549604fff7be0fad5d732a3248c0a2506458920d319b728ab03b5113fccba3933b8c3c066ff9a24976aabe9ddc5097b0e776cc2ab87da5d91daba33b72f6215a706a9daf6c4ee7f8792fbbc7c07498e8e206dfc075075a5eb5479f919';
$EXPECTED_BYTES_HEX = '6066255528d14577947f5fdb759f931591649b34f5ecaea561af452b60580784';

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
    $expectedBytes = hex2bin($EXPECTED_BYTES_HEX);
    if ($expectedBytes === false) {
        throw new RuntimeException('Invalid EXPECTED_BYTES_HEX.');
    }
    echo base64_encode(rsa_private_encrypt_pkcs1_v15($expectedBytes, $RSA_P_HEX, $RSA_Q_HEX, $RSA_E));
} catch (Throwable $e) {
    http_response_code(500);
    echo '检查更新失败，软件退出';
}
