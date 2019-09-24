import json
import cv2
import numpy as np
import paho.mqtt.client as mqtt
import boto3
from botocore.exceptions import ClientError

# jpeg layers color
BLUE, GREEN, RED = 0, 1, 2
# mqtt_broker_ip = "insert public ip of the broker"

def lambda_handler(event, context):
    
    s3 = boto3.client('s3')

    # read image from event object
    file_meta = event['Records'][0]['s3']
    event_image = file_meta['object']['key']

    bucket_name = file_meta['bucket']['name']
    image_local_path = "/tmp/"+event_image

    s3.download_file(bucket_name, event_image, image_local_path)
    
    img = cv2.imread(image_local_path)

    if img is None:
        return 1
        
    # for each color compute the sum
    red = np.sum(img[:, :, RED])
    green = np.sum(img[:, :, GREEN])
    blue = np.sum(img[:, :, BLUE])

    del img  # save some space
    # total amount of color
    base = (blue+green+red)

    # percentage of each color in the image
    red = int(red / base * 100)
    green = int(green / base * 100)
    blue = int(blue / base * 100)

    # todo if the file doesn't exist then create it
    # the json file contains the index of the images for each user
    json_name="images.json"
    json_path = "/tmp/"+json_name
    json_images = {}        # read the json file
    # if the file exists, it is downloaded and loaded, otherwise it throws ClientError
    # before it tries to open the file
    try:
        s3.download_file(bucket_name, "images.json", json_path)
        with open(json_path, 'r') as fp:
            json_images = json.load(fp)
    except ClientError:
        pass

    user_code = event_image.split("_")[0]
    img_data = {'name': event_image,
                'red': red,
                'green': green,
                'blue': blue
                }

    stop = False
    for users, macnames in json_images.items():
        for macname in macnames.keys():
            if user_code == macname:    # append new data
                json_images[users][user_code].append(img_data)
                stop = True
                break

    # insert user for the first time if not already present
    if not stop:
        json_images['users'] = {user_code: [img_data]}

    with open(json_path, 'w') as fp:
        json.dump(json_images, fp)

    # upload json file in the bucket
    response = s3.upload_file(json_path, bucket_name, json_name)

    client = mqtt.Client()
    client.connect(mqtt_broker_ip)
    client.publish(user_code, payload="{:02d}/{:02d}/{:02d}".format(red,green,blue))
    return 0
