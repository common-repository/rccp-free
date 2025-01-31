<?php
/**
 * Copyright (C) 2019-2024 Paladin Business Solutions
 *
 */

/* ================== */
/* Get RingCental SDK */
/* ================== */
function ringcentral_sdk ($settings_data = "") {
    // Include Libraries
    require('vendor/autoload.php');
    global $wpdb;

    if (is_array($settings_data)) {
        // use data from the settings / config form
        $jwt_key = $settings_data['jwt_key'];
        $ringcentral_sandbox = $settings_data['sandbox'];
    } else {
        // use DB data
        $result_rc = $wpdb->get_row($wpdb->prepare("SELECT * FROM `ringcentral_control`
        WHERE `ringcentral_control_id` = %d", 1)
        );
        $jwt_key = $result_rc->jwt_key;
        $ringcentral_sandbox = $result_rc->ringcentral_sandbox;
    }

    // Use Production platform
    $server = 'https://platform.ringcentral.com';
    $client_id = "1wCjgWSFtUucSBkvWsHuhe";
    $client_secret = "0IAEWaEmVn0c57aeOSZdDpd49WLl7U9bBe1OQaobcupG";

    $sdk = new RingCentral\SDK\SDK($client_id, $client_secret, $server);

    // Login via API
    if (!$sdk->platform()->loggedIn()) {
        try {
            $sdk->platform()->login(["jwt" => $jwt_key]);
        }
        catch (\RingCentral\SDK\Http\ApiException $e) {
            $sdk = 0;
            // exit("<br/><br/>Unable to authenticate to platform. Check your RingCentral credentials. <br/><br/>") ;
        }
    }
    return $sdk;
}

/* ================================================ */
/* Validate entered phone number has SMS permission */
/* ================================================ */
function sms_enabled_phone_number ($sdk, $phoneNum) {
    if ($sdk <> 0) {
        try {
            $responseObject = $sdk->platform()->get('/restapi/v1.0/account/~/extension/~/phone-number');
            $responseArray = json_decode($responseObject->body()->getContents(), true);

            foreach ($responseArray['records'] as $value) {
                if (in_array("SmsSender", $value['features'])) {
                    $phoneArray[] = $value['phoneNumber'];
                }
            }
        }
        catch (\RingCentral\SDK\Http\ApiException $e) {
            // Another way to get message, but keep in mind, that there could be no response if request has failed completely
            echo_spaces("API error Response", $e->getMessage());
        }

        if (in_array($phoneNum, $phoneArray)) {
            $returnValue = true;
        } else {
            $returnValue = false;
        }
    } else {
        $returnValue = false;
    }

    return $returnValue;
}

/* =============================== */
/* send out welcome email function */
/* =============================== */
function ringcentral_send_welcome_email ($email, $token, $full_name) {
    $confirm_url = add_query_arg(array('rcsubscribe' => $token, 'rcformat' => '1'), get_site_url());

    $subject = 'RingCentral - Please confirm your signup';

    $message = "Hi $full_name: <br/><br/>Thanks for joining our newsletter sign up process. ";
    $message .= "<br/><strong>[If this was not you please ignore this email]</strong><br/>";
    $message .= "If you also signed up for SMS messaging you will receive a separate confirmation text in that regard.  <br/> ";
    $message .= "<br/>Please follow this link to confirm your subscription to our email notification list: <br/> ";
    $message .= "<a href='$confirm_url'> Confirm sign up </a>";

    // Send email to new sign up email.
    $headers = array('Content-Type: text/html; charset=UTF-8');
    wp_mail($email, $subject, $message, $headers);
} // end send_welcome_email function

/* ============================= */
/* send out welcome SMS function */
/* ============================= */
function ringcentral_send_welcome_mobile ($to, $token, $full_name) {
    $message = "Hi $full_name: Thanks for joining our newsletter sign up process. [If this was not you please ignore this message]\n\n";
    $message .= "If you also signed up for email notifications you will receive a separate confirmation email in that regard.\n\n";
    $message .= "Please reply by text with 'START' to confirm your SMS subscription to our mailing list. ";

    $from = ringcentral_get_from_phone();

    $sdk = ringcentral_sdk();
    try {
        $apiResponse = $sdk->platform()->post('/account/~/extension/~/sms',
            array('from' => array('phoneNumber' => $from),
                'to' => array(array('phoneNumber' => $to)),
                'text' => $message));
    }
    catch (\RingCentral\SDK\Http\ApiException $e) {
        // In order to get Request and Response used to perform transaction:
        $apiResponse = $e->apiResponse();

        // Another way to get message, but keep in mind, that there could be no response if request has failed completely
        // print '  Message: ' . $e->apiResponse->response()->error() . PHP_EOL;
        $message = '  Message: ' . $e->apiResponse->response()->error() . PHP_EOL;
        // craft a friendly message here.
        $return_message = "There was an error adding you to our SMS list, Please try again later <br/>" . $message;
    }
} // end send_welcome_mobile function

/* ============================================== */
/* generate unique ID for new subscriber function */
/* ============================================== */
function ringcentral_unique_token () {
    global $wpdb;
    $result_rc = $wpdb->get_row($wpdb->prepare("SELECT `token_prefix`
            FROM `ringcentral_control` WHERE `ringcentral_control_id` = %d", 1)
    );
    // $prefix = $result_rc->token_prefix ;
    return uniqid($result_rc->token_prefix, true);
}

/* ======================================== */
/* get from phone number from control table */
/* it is stored in the user_name field      */
/* ======================================== */
function ringcentral_get_from_phone () {
    global $wpdb;
    // `ringcentral_user_name` is the phone number for the app owner
    $result_rc = $wpdb->get_row($wpdb->prepare("SELECT `ringcentral_user_name`
            FROM `ringcentral_control` WHERE `ringcentral_control_id` = %d", 1)
    );
    return $result_rc->ringcentral_user_name;
}

/* ========================= */
/* call ringout api function */
/* ========================= */
function ringcentral_ringout ($to) {
    $sdk = ringcentral_sdk();
    $from = ringcentral_get_from_phone();
    try {
        $apiResponse = $sdk->platform()->post('/account/~/extension/~/ringout',
            array(
                // dynamic eventually
                'from' => array('phoneNumber' => $from),
                // number from the widget form
                'to' => array('phoneNumber' => $to))
        );
    }
    catch (\RingCentral\SDK\Http\ApiException $e) {

        // In order to get Request and Response used to perform transaction:
        $apiResponse = $e->apiResponse();

        // craft a friendly message here.
        // $return_message = "There was an error adding you to our SMS list, please contact us or try again later";
    }
}

/* ============================== */
/* Encrypt Password to Obfusicate */
/* ============================== */
function ringcentral_encrypt ($plaintext) {
    if (function_exists('openssl_cipher_iv_length')) {
        $key = $_SERVER['SERVER_NAME'] . $_SERVER['SERVER_ADDR'] . $_SERVER['SERVER_SOFTWARE'];
        $cipher = "AES-128-CBC";
        $ivlen = openssl_cipher_iv_length($cipher);
        $iv = substr($c, 0, $ivlen);
        return base64_encode(openssl_encrypt($plaintext, $cipher, $key));
    }
    return $plaintext;
}

/* ============================== */
/* Decrypt Password to Obfusicate */
/* ============================== */
function ringcentral_decrypt ($ciphertext) {
    if (function_exists('openssl_cipher_iv_length')) {
        $key = $_SERVER['SERVER_NAME'] . $_SERVER['SERVER_ADDR'] . $_SERVER['SERVER_SOFTWARE'];
        $cipher = "AES-128-CBC";
        return openssl_decrypt(base64_decode($ciphertext), $cipher, $key);
    } else {
        return $ciphertext;
    }
}

/* ============================== */
/* Build help icon and title text */
/* ============================== */
function ringcentral_build_help ($field) {
    global $wpdb;
    $image_source = RINGCENTRAL_PLUGINURL . 'images/question_mark.png';

    $result_rc_help = $wpdb->get_row($wpdb->prepare("SELECT ringcentral_help_help AS help_text
            FROM `ringcentral_help` WHERE `ringcentral_help_field` = %s", $field));

    $out_string = "<img src='$image_source' title='" . esc_attr($result_rc_help->help_text) . "' />";
    return $out_string;
}

/* =================== */
/* Build ReCaptcha top */
/* =================== */
function ringcentral_build_recaptcha_top () {
    echo "<script src='https://www.google.com/recaptcha/api.js' async defer></script>";
    return;
}

/* ==================== */
/* Build ReCaptcha main */
/* ==================== */
function ringcentral_build_recaptcha_main () {
    global $wpdb;
    $result_rc = $wpdb->get_row($wpdb->prepare("SELECT `grc_site_key`
            FROM `ringcentral_control` WHERE `ringcentral_control_id` = %d", 1)
    );
    $site_key = $result_rc->grc_site_key;
    echo "<tr class='TableOverride'><td>&nbsp;</td><td ><br/>";
    echo "<div class='g-recaptcha' data-sitekey='" . $site_key . "'></div>";
    echo "</td></tr>";
    return;
}

/* ========================= */
/* Verify ReCaptcha response */
/* ========================= */
function ringcentral_verify_recaptcha ($action) {
    global $wpdb;
    $result_rc = $wpdb->get_row($wpdb->prepare("SELECT `grc_secret_key`
            FROM `ringcentral_control` WHERE `ringcentral_control_id` = %d", 1)
    );
    $secret_key = $result_rc->grc_secret_key;
    $url = 'https://www.google.com/recaptcha/api/siteverify';
    $data = array(
        'secret' => $secret_key,
        'response' => $action
    );
    $options = array(
        'http' => array(
            'method' => 'POST',
            'content' => http_build_query($data)
        )
    );
    $context = stream_context_create($options);
    $verify = file_get_contents($url, false, $context);
    $captcha_success = json_decode($verify);
    if ($captcha_success->success == false) {
        $response = false;
    } elseif ($captcha_success->success == true) {
        $response = true;
    }
    return $response;
}

/* ============================= */
/* show / hide "Get Pro" button  */
/* ============================= */
function ringcentral_CheckPro () {
    global $wpdb;

    $remote_string = "https://paladin-bs.com/check_rccp_free_pro.php";
    $args = array('timeout' => 5);

    return $showHide = wp_remote_retrieve_body(wp_remote_get($remote_string, $args));
}

/* ============================================= */
/* Get list of Team Messaging Group Chats (Teams) */
/* ============================================= */
function list_tm_teams ($rcsdk) {

    $platform = $rcsdk->platform();

    $queryParams = array(
        'type' => array('Team'),
        //'type' => array( 'Everyone', 'Group', 'Personal', 'Direct', 'Team' ),
        //'recordCount' => 2,
    );

    try {
        $response = $platform->get("/restapi/v1.0/glip/chats", $queryParams);
    }
    catch (\RingCentral\SDK\Http\ApiException $e) {
        $response = false;
        // $response = $e ;
    }

    return $response;
}

/* ============================================== */
/* Post text to Team Messaging Group Chats (Teams) */
/* ============================================== */
function post_tm_chat ($rcsdk, $chatId, $team_chat) {

    $platform = $rcsdk->platform();

    $body = array(
        'text' => $team_chat,
        // use options below if you are ever going to send attachments to the chat as well.
        // 'attachments' => array(
        //	array(
        //		'id' => '<ENTER VALUE>',
        //		'type' => '<ENTER VALUE>'
        //	)
    );

    try {
        $platform->post("/restapi/v1.0/glip/chats/{$chatId}/posts", $body);
        $result = true;
    }
    catch (\RingCentral\SDK\Http\ApiException $e) {
        // echo "<br/><br/>Failed to add text to selected Team chat -><br/>" . $e ;
        $result = false;
    }
    return $result;
}

function ringcentral_admin_page_top () { ?>
    <img id='page_title_img' title="RingCentral Plugin" src="<?= RINGCENTRAL_LOGO; ?>">
    &nbsp;&nbsp;
    <h1 id='page_title'><?php echo esc_html(get_admin_page_title()); ?></h1>
<?php }

function ringcentral_plugin_version () {
    echo "<p style='font-size: small'> RCCP Free Version: " . RINGCENTRAL_PLUGIN_VERSION . "</p>";
} ?>
