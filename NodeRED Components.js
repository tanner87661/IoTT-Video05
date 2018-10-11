[{"id":"62c26ace.508184","type":"tab","label":"Reflow Oven Control","disabled":false,"info":""},{"id":"f5181929.9714f8","type":"mqtt out","z":"62c26ace.508184","name":"Heat Control","topic":"rflCmd","qos":"1","retain":"true","broker":"5bc7cc44.39f4b4","x":796,"y":131,"wires":[]},{"id":"acf093bc.29021","type":"function","z":"62c26ace.508184","name":"Heat Control","func":"if (msg.topic == \"PWMLevel\")\n    context.set(\"PWMLevel\", msg.payload);\nif (msg.topic == \"WorkMode\")\n    context.set(\"WorkMode\", msg.payload);\nif (msg.topic == \"TargetTemp\")\n    context.set(\"TargetTemp\", msg.payload);\nif (msg.topic == \"CurveSel\")\n    context.set(\"CurveSel\", msg.payload);\n\nvar workMode = context.get(\"WorkMode\");\nif (workMode === undefined) \n    workMode = \"0\";\nvar pwmLevel = context.get(\"PWMLevel\");\nif (pwmLevel === undefined) \n    pwmLevel = 0;\nvar targetTemp = context.get(\"TargetTemp\");\nif (targetTemp === undefined) \n    targetTemp = 0;\nvar activeProfile = context.get(\"CurveSel\");\nif (activeProfile === undefined) \n    activeProfile = -1;\n\nvar payloadStr = \"{\\\"WorkMode\\\":\\\"\" +  workMode + \"\\\"\";\nswitch (workMode)\n{\n    case 0: \n        break;\n    case 1: //PWM\n        payloadStr += \", \\\"HeatLevel\\\": \" + pwmLevel;\n        break;\n    case 2: //PID\n        payloadStr += \", \\\"TargetTemp\\\": \" + targetTemp;\n        break;\n    case 3: //PID\n        payloadStr += \", \\\"SelectProfile\\\": \" + activeProfile;\n        if (msg.topic == \"StartReflow\")\n            payloadStr += \", \\\"RunProfile\\\": 1\";\n        else\n            payloadStr += \", \\\"RunProfile\\\": 0\";\n        break;\n}\npayloadStr += \"}\"\n\n\nmsg = {payload:payloadStr}\nreturn msg;\n","outputs":1,"noerr":0,"x":505.00002670288086,"y":199.00001907348633,"wires":[["f5181929.9714f8","4f9f73c5.92389c"]]},{"id":"4f9f73c5.92389c","type":"debug","z":"62c26ace.508184","name":"","active":false,"tosidebar":true,"console":false,"tostatus":false,"complete":"false","x":1032,"y":200,"wires":[]},{"id":"b4a3c4.e91bec4","type":"ui_dropdown","z":"62c26ace.508184","name":"","label":"Work Mode","place":"Select option","group":"51790e44.1eabe","order":1,"width":"12","height":"1","passthru":false,"options":[{"label":"OFF","value":0,"type":"num"},{"label":"PWM","value":1,"type":"num"},{"label":"PID","value":2,"type":"num"},{"label":"Reflow Curve","value":3,"type":"num"}],"payload":"","topic":"WorkMode","x":173.07987213134766,"y":154.93063354492188,"wires":[["acf093bc.29021"]]},{"id":"1370113b.1839bf","type":"ui_slider","z":"62c26ace.508184","name":"","label":"Target Temperature","group":"51790e44.1eabe","order":4,"width":"12","height":"1","passthru":true,"topic":"TargetTemp","min":0,"max":"250","step":1,"x":188.07638549804688,"y":249.8577117919922,"wires":[["acf093bc.29021"]]},{"id":"85b3eee7.92137","type":"ui_slider","z":"62c26ace.508184","name":"","label":"PWM Level","group":"51790e44.1eabe","order":3,"width":"12","height":"1","passthru":true,"topic":"PWMLevel","min":0,"max":"100","step":1,"x":170.0833282470703,"y":198.8611297607422,"wires":[["acf093bc.29021"]]},{"id":"94b8e4bb.852488","type":"ui_dropdown","z":"62c26ace.508184","name":"","label":"Select Curve","place":"Select Curve","group":"51790e44.1eabe","order":2,"width":"12","height":"1","passthru":true,"options":[{"label":"Lead (Sn63 Pb37)","value":0,"type":"num"},{"label":"Lead-free (SAC305)","value":1,"type":"num"}],"payload":"","topic":"CurveSel","x":166,"y":302,"wires":[["acf093bc.29021"]]},{"id":"babc2940.c8ec68","type":"mqtt in","z":"62c26ace.508184","name":"","topic":"dataReflow","qos":"1","broker":"5bc7cc44.39f4b4","x":149.00000762939453,"y":457.99998664855957,"wires":[["708bed58.415e84"]]},{"id":"d2f441d1.15756","type":"ui_chart","z":"62c26ace.508184","name":"","group":"51790e44.1eabe","order":6,"width":"24","height":"8","label":"Current Temperature","chartType":"line","legend":"true","xformat":"HH:mm:ss","interpolate":"linear","nodata":"","dot":false,"ymin":"0","ymax":"300","removeOlder":"15","removeOlderPoints":"","removeOlderUnit":"60","cutout":0,"useOneColor":false,"colors":["#1f77b4","#aec7e8","#ff7f0e","#2ca02c","#98df8a","#d62728","#ff9896","#9467bd","#c5b0d5"],"useOldStyle":false,"x":776.632080078125,"y":426.96697998046875,"wires":[[],[]]},{"id":"708bed58.415e84","type":"json","z":"62c26ace.508184","name":"","property":"payload","action":"","pretty":false,"x":341.632080078125,"y":457.96697998046875,"wires":[["a2efa80a.fb67c8","61a71f78.4123c"]]},{"id":"a2efa80a.fb67c8","type":"function","z":"62c26ace.508184","name":"Output","func":"if (msg.payload.CurrTempC >= 0)\n    msg1 = {payload:msg.payload.CurrTempC}\nelse\n    msg1 = null\nif (msg.payload.SetTempC >= 0)\n    msg2 = {payload:msg.payload.SetTempC}\nelse\n    msg2 = null\nif (msg.payload.HeatLevel >= 0 )\n    msg3 = {payload:msg.payload.HeatLevel}\nelse\n    msg3 = null\nif (msg.payload.HeaterStatus >= 0)\n    msg4 = {payload:msg.payload.HeaterStatus}\nelse\n    msg4 = null\nif (msg.payload.VoiceMsg > 0)\n    switch (msg.payload.VoiceMsg)\n    {\n        case 1: msg5 = {payload:\"Please open the oven and let it cool off\"}; break;\n        case 2: msg5 = {payload:\"Profile Completed. Please remove and verify parts\"}; break;\n        default: msg5 = null;\n    }\n    \nelse\n    msg5 = null\n    \nreturn [msg1, msg2, msg3, msg4, msg5];","outputs":5,"noerr":0,"x":523.6320877075195,"y":458.9669704437256,"wires":[["d2f441d1.15756"],["b0dc9a6f.427168"],["1c7c3db0.07e7d2"],["57278ce.5a8f574"],["f295615b.6cf23"]]},{"id":"1c7c3db0.07e7d2","type":"ui_gauge","z":"62c26ace.508184","name":"","group":"51790e44.1eabe","order":7,"width":"8","height":"4","gtype":"gage","title":"PWM %","label":"%","format":"{{value}}","min":0,"max":"100","colors":["#00b500","#e6e600","#ca3838"],"seg1":"","seg2":"","x":737.7050552368164,"y":496.7968330383301,"wires":[]},{"id":"b0dc9a6f.427168","type":"ui_gauge","z":"62c26ace.508184","name":"","group":"51790e44.1eabe","order":9,"width":"8","height":"4","gtype":"gage","title":"Target Temp","label":"Deg C","format":"{{value}}","min":0,"max":"300","colors":["#00b500","#e6e600","#ca3838"],"seg1":"","seg2":"","x":744.7050514221191,"y":461.8072500228882,"wires":[]},{"id":"61a71f78.4123c","type":"debug","z":"62c26ace.508184","name":"","active":false,"tosidebar":true,"console":false,"tostatus":false,"complete":"false","x":1033,"y":245,"wires":[]},{"id":"3a7066f8.2ce60a","type":"ui_button","z":"62c26ace.508184","name":"Start Reflow","group":"51790e44.1eabe","order":5,"width":"3","height":"1","passthru":false,"label":"Start Reflow","color":"","bgcolor":"","icon":"","payload":"1","payloadType":"num","topic":"StartReflow","x":165,"y":346,"wires":[["acf093bc.29021"]]},{"id":"f295615b.6cf23","type":"ui_audio","z":"62c26ace.508184","name":"","group":"51790e44.1eabe","voice":"en-US","always":true,"x":738,"y":580,"wires":[]},{"id":"8fbefaf6.636bf8","type":"mqtt in","z":"62c26ace.508184","name":"","topic":"pingReflow","qos":"1","broker":"5bc7cc44.39f4b4","x":151.00000762939453,"y":399,"wires":[["4f9f73c5.92389c"]]},{"id":"57278ce.5a8f574","type":"ui_template","z":"62c26ace.508184","group":"51790e44.1eabe","name":"Oven ON","order":8,"width":"8","height":"4","format":"<md-button class=\"vibrate filled touched bigfont rounded\" style=\"background-color:#FFFFFF\" ng-click=\"send({payload: msg.payload })\"> \n\n\n<svg  width=\"260px\" height=\"90px\" version=\"1.1\" viewBox=\"0 0 800 200\">\n <g id=\"Button_Long\">\n  \n  <rect fill=\"#FFFFFF\" width=\"800\" height=\"200\"/>\n  <g ng-style=\"{fill: (msg.payload || 0) ? 'lime' : 'red'}\">\n    <rect width=\"800\" height=\"200\" rx=\"80\" ry=\"80\"/>\n  </g>\n  \n  <g ng-style=\"{fill: (msg.payload || 0) ? 'lime' : 'red'}\">\n    <rect x=\"11\" y=\"10\" width=\"778\" height=\"180\" rx=\"90\" ry=\"90\"/>\n  </g>\n  <g ng-style=\"{fill: (msg.payload || 0) ? 'black' : 'white'}\">\n      \n    <text x=\"400\" y=\"125\" style=\"text-anchor:middle\"  font-weight=\"bold\" font-size=\"80\" font-family=\"Arial\">{{(msg.payload||0)? \"ON\" : \"OFF\"}} </text>\n    </g>\n  </g>\n</svg>\n\n\n</md-button>\n","storeOutMessages":false,"fwdInMessages":false,"templateScope":"local","x":738,"y":539,"wires":[[]]},{"id":"5bc7cc44.39f4b4","type":"mqtt-broker","z":"","name":"","broker":"192.168.87.52","port":"1883","clientid":"","usetls":false,"compatmode":true,"keepalive":"60","cleansession":true,"birthTopic":"","birthQos":"0","birthRetain":"false","birthPayload":"","willTopic":"","willQos":"0","willPayload":""},{"id":"51790e44.1eabe","type":"ui_group","z":"","name":"Reflow Oven Control","tab":"31a516b3.bd79ca","order":1,"disp":true,"width":"24","collapse":false},{"id":"31a516b3.bd79ca","type":"ui_tab","z":"","name":"Reflow Oven","icon":"dashboard","order":5}]
