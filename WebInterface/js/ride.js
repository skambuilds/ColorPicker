/*global WildRydes _config*/

var WildRydes = window.WildRydes || {};
WildRydes.map = WildRydes.map || {};
var bucketUrl = 'your_bucket_url';
var jsonName = 'images.json';

(function rideScopeWrapper($) {
    var authToken;
    WildRydes.authToken.then(function setAuthToken(token) {
        if (token) {
            authToken = token;
        } else {
            window.location.href = '/signin.html';
        }
    }).catch(function handleTokenError(error) {
        alert(error);
        window.location.href = '/signin.html';
    });
    function requestUnicorn(pickupLocation) {
        $.ajax({
            method: 'POST',
            url: _config.api.invokeUrl + '/ride',
            headers: {
                Authorization: authToken
            },
            data: JSON.stringify({
                PickupLocation: {
                    Latitude: pickupLocation.latitude,
                    Longitude: pickupLocation.longitude
                }
            }),
            contentType: 'application/json',
            success: completeRequest,
            error: function ajaxError(jqXHR, textStatus, errorThrown) {
                console.error('Error requesting ride: ', textStatus, ', Details: ', errorThrown);
                console.error('Response: ', jqXHR.responseText);
                alert('An error occured when requesting your unicorn:\n' + jqXHR.responseText);
            }
        });
    }

    function completeRequest(result) {
        var unicorn;
        var pronoun;
        console.log('Response received from API: ', result);
        unicorn = result.Unicorn;
        pronoun = unicorn.Gender === 'Male' ? 'his' : 'her';
        displayUpdate(unicorn.Name + ', your ' + unicorn.Color + ' unicorn, is on ' + pronoun + ' way.');
        animateArrival(function animateCallback() {
            displayUpdate(unicorn.Name + ' has arrived. Giddy up!');
            WildRydes.map.unsetLocation();
            $('#request').prop('disabled', 'disabled');
            $('#request').text('Set Pickup');
        });
    }

    // Register click handler for #request button
    $(function onDocReady() {
        $('#request').click(handleRequestClick);
        $('#signOut').click(function() {
            WildRydes.signOut();
            alert("You have been signed out.");
            window.location = "signin.html";
        });
        $(WildRydes.map).on('pickupChange', handlePickupChanged);
/*
        WildRydes.authToken.then(function updateAuthMessage(token) {
            if (token) {
                displayUpdate('You are authenticated. Click to see your <a href="#authTokenModal" data-toggle="modal">auth token</a>.');
                $('.authToken').text(token);
            }
        });
*/
        WildRydes.attribute.then(function updateAttributeMessage(attr) {
            //if (attr) {
                displayUpdate('You are authenticated. Click to see your <a href="#codeNumberModal" data-toggle="modal">device number</a>.');
                $('.codeNumber').text(attr);

                
                 loadJSON(function(response) {
                  // Parsing JSON string into object

                    var actual_JSON = JSON.parse(response);
                    console.log(actual_JSON);
                    var htmlTemplate = [];                    
                    var imageNumber = actual_JSON.users[attr].length;
                    var rowCounter = 0;
                    for (i = 0; i < imageNumber; i++) {
                        rowCounter++;
                        photoName = actual_JSON.users[attr][i].name;
                        var red = actual_JSON.users[attr][i].red * 255 / 100;
                        var green = actual_JSON.users[attr][i].green * 255 / 100;
                        var blue = actual_JSON.users[attr][i].blue * 255 / 100;
                        var rowOpen = '';
                        var rowCloseOpen = '';
                        var rowClose = '';

                        if (i==0) { rowOpen = '<div class="row">'}
                        if (rowCounter % 3 == 0) { rowCloseOpen = '</div><div class="row">'}
                        if (i==imageNumber-1) { rowClose = '</div>'}

                        var singleHtmlTemplate = [
                        rowOpen,
                        '<div class="columns large-4">',
                            '<div class="three block">',
                                '<div class="imageblock">',
                                    '<img class="imagesample" src="' + bucketUrl + photoName + '"/>',
                                    '<div class="overlayimage" style="background-color:rgb(' + red + ', ' + green + ', ' + blue + ');">',
                                    '<div class="textimage">RGB: ' + actual_JSON.users[attr][i].red + '-' + actual_JSON.users[attr][i].green + '-' + actual_JSON.users[attr][i].blue + '</div>',
                                '</div>',
                            '</div>',
                            '<h3 class="title">Immagine: ' + photoName + '</h3>',
                            '</div>',
                        '</div>',
                        rowCloseOpen,
                        rowClose,
                        ]
                        console.log(singleHtmlTemplate);
                        var htmlTemplate = htmlTemplate.concat(singleHtmlTemplate);
                        
                    } 
                    console.log(htmlTemplate);
                    document.getElementById('viewer').innerHTML = getHtml(htmlTemplate);
                 });
                

            //}
        });

        if (!_config.api.invokeUrl) {
            $('#noApiMessage').show();
        }
    });


    // A utility function to create HTML.
    function getHtml(template) {
      return template.join('\n');
    }
    
    function loadJSON(callback) {
        var jsonPath = bucketUrl + jsonName;   
        var xobj = new XMLHttpRequest();
            xobj.overrideMimeType("application/json");
        xobj.open('GET', jsonPath, true); // Replace 'appDataServices' with the path to your file
        xobj.onreadystatechange = function () {
              if (xobj.readyState == 4 && xobj.status == "200") {
                // Required use of an anonymous callback as .open will NOT return a value but simply returns undefined in asynchronous mode
                callback(xobj.responseText);
              }
        };
        xobj.send(null);  
    }

    function handlePickupChanged() {
        var requestButton = $('#request');
        requestButton.text('Request Unicorn');
        requestButton.prop('disabled', false);
    }

    function handleRequestClick(event) {
        var pickupLocation = WildRydes.map.selectedPoint;
        event.preventDefault();
        requestUnicorn(pickupLocation);
    }

    function animateArrival(callback) {
        var dest = WildRydes.map.selectedPoint;
        var origin = {};

        if (dest.latitude > WildRydes.map.center.latitude) {
            origin.latitude = WildRydes.map.extent.minLat;
        } else {
            origin.latitude = WildRydes.map.extent.maxLat;
        }

        if (dest.longitude > WildRydes.map.center.longitude) {
            origin.longitude = WildRydes.map.extent.minLng;
        } else {
            origin.longitude = WildRydes.map.extent.maxLng;
        }

        WildRydes.map.animate(origin, dest, callback);
    }

    function displayUpdate(text) {
        $('#updates').append($('<li>' + text + '</li>'));
    }
}(jQuery));
