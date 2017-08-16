# You may have to install Pillow for this program:
# pip2.7 install -I --no-cache-dir Pillow


from time import sleep
import serial
import threading
import Tkinter as tk
from PyQt4 import QtCore
from PIL import Image,ImageTk
 

   
class SensorThread(threading.Thread):
    def run(self):
        global serialdata
        while not terminate:
            try:
                if ser.inWaiting():
                    serialdata = ser.readline()
                    #print serialdata,' '
            except Exception as e:
                print e
#--------------------------------------------------------------------            

serialdata = "0"  # globally shared
terminate = False
TIMEOUT = 1
baud = 9600
#ser = serial.Serial('/dev/ttyACM0', baud, timeout=0)
ser = serial.Serial('COM7', baud, timeout=0)
#ser  = serial.Serial('COM7', baud, timeout=TIMEOUT)

try:
    ser.flushInput()  
    ser.flushOutput() 
except Exception as ex:
    print (ex)    
    ser.close()
     
SensorThread().start()
#Gui().go()
 
try: 
    while not terminate:
        data = serialdata.strip()
        n = len(data)
        if (n > 0):
            time = QtCore.QTime.currentTime()
            text = time.toString('hh:mm:ss')
            #print text, '  ', data
except KeyboardInterrupt:
    terminate = True
    
#f.close()
ser.close()
print '\n', 'Done!'	
sleep(2)  # time.sleep(seconds)
exit()

''' 
root = tk.Tk()
img1 = ImageTk.PhotoImage(Image.open('red_led.jpg'))
img2 = ImageTk.PhotoImage(Image.open('green_led.jpg'))
imglabel = tk.Label(root, image = img1)
imglabel.pack(side = "bottom", fill = "both", expand = "yes")
if flag:
    imglabel.configure(image=img1)
else:
    imglabel.configure(image=img2)
 
----- 
try:
    inp = ser.read()
    #inp = ser.readLine();
except Exception as ex:

except serial.SerialTimeoutException:


f = open('radar1.csv', 'wt')
f.write('time,status\n')
'''


  