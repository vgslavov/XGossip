#!/bin/bash

# Function to evaluate the absolute value of a number
abs () {
	if [ $1 -lt 0 ] ; then
                echo "0 - $1" | bc -l
        else
                echo $1
        fi 
return 0 # OK

}

A=-10; abs $A
A=121; abs $A
A=1x1; abs $A
A=-0.00004; abs $A
