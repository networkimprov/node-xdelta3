{
  'targets': [
    {
      'target_name': 'node_xdelta3',
      'sources': [ 
        'src/node_xdelta3.cc',
        'src/internal/FileReader.cc'
      ],
      'link_settings': {
        'libraries': [ '-lxdelta3' ]
      }
    }
  ]
}
