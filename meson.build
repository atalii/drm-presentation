project('drm-presentation', 'c',
  version : '0.1',
  default_options : ['warning_level=3'])

libdrm = dependency('libdrm')

executable('00-open-card',
           '00-open-card.c',
           install : true)

executable('01-get-resources',
           '01-get-resources.c',
           dependencies : [ libdrm ],
           install : true)

executable('02-get-connectors',
           '02-get-connectors.c',
           dependencies : [ libdrm ],
           install : true)

executable('03-get-crtc',
           '03-get-crtc.c',
           dependencies : [ libdrm ],
           install : true)

executable('04-get-mode',
           '04-get-mode.c',
           dependencies : [ libdrm ],
           install : true)

executable('05-get-plane',
           '05-get-plane.c',
           dependencies : [ libdrm ],
           install : true)

executable('06-get-buf',
           '06-get-buf.c',
           dependencies : [ libdrm ],
           install : true)

executable('07-draw',
           '07-draw.c',
           dependencies : [ libdrm ],
           install : true)
