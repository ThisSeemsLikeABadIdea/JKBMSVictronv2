#include "htmlcomponents.h"
#include <stdlib.h>
#include <string.h>

//const int htmlBufferSize = 3096;

char wifi_provisioning_page[5120];

//static const char css_content[] = ;
//    "<link href='https://maxcdn.bootstrapcdn.com/bootstrap/4.5.2/css/bootstrap.min.css' rel='stylesheet'>"
    
static const char  html_headers[]  = "<html><head>"
        
           "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.5.1/jquery.min.js'></script>"
           "<script src='https://cdnjs.cloudflare.com/ajax/libs/popper.js/1.16.0/umd/popper.min.js'></script>"
           "<script src='https://maxcdn.bootstrapcdn.com/bootstrap/4.5.2/js/bootstrap.min.js'></script>"
           " <style> \n .card-body > form {text-align: center;\n}\n"    
            ".card {\n width: 18rem;\n}\n"    
            ".card-header {\n"
            "text-align: center;\n}\n"
            ".container { min-width:992px!important; }"
            ".col-sm { -ms-flex-preferred-size:0;flex-basis:0;-ms-flex-positive:1;flex-grow:1;max-width:100%; }"
            ".card { position:relative;display:-ms-flexbox;display:flex;-ms-flex-direction:column;flex-direction:column;min-width:0;word-wrap:break-word;background-color:#fff;background-clip:border-box;border:1px solid rgba(0,0,0,.125);border-radius:.25rem; }"
            ".card-body { -ms-flex:1 1 auto;flex:1 1 auto;min-height:1px;padding:1.25rem; }"
            ".card-header { padding:.75rem 1.25rem;margin-bottom:0;background-color:rgba(0,0,0,.03);border-bottom:1px solid rgba(0,0,0,.125); }"
            ".row { display:-ms-flexbox;display:flex;-ms-flex-wrap:wrap;flex-wrap:wrap;margin-right:-15px;margin-left:-15px; }"
            "body { margin: 0; font-family: -apple-system, BlinkMacSystemFont, \"Segoe UI\", Roboto, \"Helvetica Neue\", Arial, \"Noto Sans\", sans-serif, \"Apple Color Emoji\", \"Segoe UI Emoji\", \"Segoe UI Symbol\", \"Noto Color Emoji\"; font-size: 1rem; font-weight: 400; line-height: 1.5; color: #212529; text-align: left; } </style>"            
            "</head>";



static const char html_body[] = "<body><p> </p><div class=\"container\"><div class=\"row\"><div class=\"col-sm\">";

static const char html_wifistatus_card[] = "<div class=\"card\" style=\"width: 18rem;\"><h5 class=\"card-header\" style=\"text-align: center\">WiFi Status</h5><h6 class=\"card-header\" style=\"text-align: center\">Connected</h6><div class=\"card-body\"><h4> wifi name </h4><ul id=\"wifi_list\"></ul></div>";

static const char html_card_breaker[] = "</div></div><div class=\"col-sm\">";

static const char html_connectForm[] =  "<div class=\"card\" style=\"width: 18rem;\"><h5 class=\"card-header\" style=\"text-align: center\">WiFi Provisioning</h5><div class=\"card-body\">"
            "<form action='/configure' method='post'> "
            "WiFi Name: <input type='text' name='wifi_name' id='wifi_name'><br>"
            "WiFi Password: <input type='password' name='wifi_password'><br> <p> </p> "
            "<input type='submit' value='Submit'>"
            "</form> </div>";

static const char html_mqttForm[] = "<div class=\"card\" style=\"width: 18rem;\">"
            "<h5 class=\"card-header\" style=\"text-align: center\">MQTT Provisioning</h5><div class=\"card-body\">"
            "<form action=\"/mqttconfigure\" method=\"post\" style=\"text-align: center\">MQTT Host: <input type=\"text\" name=\"host_Name\" id=\"mqtt_Host\">"
            "<br>MQTT Port: <input type=\"text\" name=\"mqtt_Port\" id=\"mqtt_Port\"><br><p> </p><input type=\"submit\" value=\"Submit\"></form></div></div>";
static const char html_commandForm[] = "<div class=\"card\" style=\"width: 18rem;\">"
            "<h5 class=\"card-header\" style=\"text-align: center\">Commands</h5><div class=\"card-body\">"
            "<form id=\"restartForm\" method=\"post\" action=\"/api/command\">"
            "<input type=\"hidden\" name=\"command\" value=\"restart\">"
            "<button type=\"submit\">Restart</button>"
            "</form>"
            "<br><br><p> </p></div></div>";

static const char html_end[] = "</div></div></div></div></div></body><script>" 
"// Function to fetch MQTT host and port\n"
"function fetchMQTTConfig() {\n"
"    // Assuming you have an endpoint to fetch MQTT configuration\n"
"    fetch('/mqtt_status')\n"
"    .then(response => response.json())\n"
"    .then(data => {\n"
"        console.log('MQTT Host:', data.mqtt_host);\n"
"        console.log('MQTT Port:', data.mqtt_port);\n"
"       var mqttHostInput = document.getElementById(\"mqtt_Host\"); "
"       var mqttPortInput = document.getElementById(\"mqtt_Port\"); "
"if (!mqttHostInput.getAttribute(\"data-typing\") && !mqttPortInput.getAttribute(\"data-typing\")) {"
"            mqttHostInput.value = data.mqtt_host;"
"            mqttPortInput.value = data.mqtt_port;"
"        } "
"    })\n"
"    .catch(error => {\n"
"        console.error('Error fetching MQTT configuration:', error);\n"
"    });\n"
"}\n"
"\n"
"// Function to fetch list of available Wi-Fi networks\n"
"function fetchWifiNetworks() {\n"
"    // Assuming you have an endpoint to fetch Wi-Fi networks\n"
"    fetch('/wifi_networks')\n"
"    .then(response => response.json())\n"
"    .then(data => {\n"
"        console.log('Available Wi-Fi Networks:', data);\n"
"    })\n"
"    .catch(error => {\n"
"        console.error('Error fetching Wi-Fi networks:', error);\n"
"    });\n"
"}\n"
"\n"
"// Function to fetch MQTT configuration and Wi-Fi networks every 5 seconds\n"
"function autoUpdate() {\n"
"    fetchMQTTConfig();\n"
"    fetchWifiNetworks();\n"
"    setTimeout(autoUpdate, 5000); // Call autoUpdate function recursively every 5 seconds\n"
"}\n"
"\n"
"   document.getElementById(\"mqtt_Host\").addEventListener(\"input\", function() {this.setAttribute(\"data-typing\", true);    });"
"   document.getElementById(\"mqtt_Port\").addEventListener(\"input\", function() {this.setAttribute(\"data-typing\", true);    });"
"// Call autoUpdate function to start auto-updating\n"
"autoUpdate();\n"
"</script></html>";

const char* get_wifi_provisioning_page()
{
    Clear_html_buffer();
    add_wifi_prov_headers();
    add_html_body();
    add_wifi_status_box();
    add_card_breaker();    
    add_html_wifi_Form();
    add_card_breaker();    
    add_mqtt_form();
    add_card_breaker();    
    add_command_box();
    add_card_breaker();    
    add_html_end();
    return wifi_provisioning_page;
    }

void add_command_box(){    
    strcat(wifi_provisioning_page,html_commandForm);
}

void add_wifi_status_box(){ 
    strcat(wifi_provisioning_page,html_wifistatus_card);
 };

void add_card_breaker(){ 
    strcat(wifi_provisioning_page, html_card_breaker);
    }; 

void add_mqtt_form(){ 
     strcat(wifi_provisioning_page,  html_mqttForm);
     }; 

void Clear_html_buffer()
{
    memset(wifi_provisioning_page, '\0', sizeof(wifi_provisioning_page)); 
}

void add_wifi_prov_headers() {
    // Concatenate html_headers to wifi_provisioning_page
    strcat(wifi_provisioning_page, html_headers);
}

void add_html_body() {
    // Concatenate html_body to wifi_provisioning_page
    strcat(wifi_provisioning_page, html_body);
}

void add_html_wifi_Form() {
    // Concatenate html_connectForm to wifi_provisioning_page
    strcat(wifi_provisioning_page, html_connectForm);
}

void add_html_end()
{
    // Concatenate html_end to wifi_provisioning_page
    strcat(wifi_provisioning_page, html_end);
}
//html_connectForm[]