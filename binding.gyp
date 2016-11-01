{
  'targets': [
    {
      'target_name': 'node_xdelta3',
      'sources': [ 
        'src/node_xdelta3.cc',
        'src/internal/FileReader.cc',
        'src/internal/FileWriter.cc',
        'src/internal/XdeltaOp.cc',
        'src/XdeltaPatch.cc',
        'src/XdeltaDiff.cc'
      ],
      'link_settings': {
        'libraries': [ '-lxdelta3' ]
      }
    }
  ]
}
