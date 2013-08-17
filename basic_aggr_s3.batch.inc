<?php
/**
 * @file
 * basic_aggr_s3.batch.inc
 * @copyright The Royal National Theatre
 * @author John-Paul Drawneek
 */

/**
 * S3 Refresh form.
 *
 * @param array $form
 *   Current for structure.
 * @param array $form_state
 *   Current form state.
 *
 * @return array
 *   Form structure to render.
 */
function basic_aggr_s3_force_refresh_form($form, &$form_state) {
  form_load_include($form_state, 'inc', 'basic_aggr_s3', 'basic_aggr_s3.batch');
  $form = array();
  $form['full'] = array(
    '#type'  => 'fieldset',
    '#title' => t('Full import'),
  );
  $form['full']['all'] = array(
    '#type'     => 'submit',
    '#value'    => t('Import All'),
    '#submit'   => array('basic_aggr_s3_force_refresh_all'),
  );
  $form['full']['new'] = array(
    '#type'     => 'submit',
    '#value'    => t('Import New'),
    '#submit'   => array('basic_aggr_s3_force_refresh_new'),
  );

  $form['truncate'] = array(
    '#type'        => 'fieldset',
    '#title'       => t('Drop All'),
    '#description' => t('Clear log table, repopulate via cron.'),
  );
  $form['truncate']['go'] = array(
    '#type'     => 'submit',
    '#value'    => t('Drop All'),
    '#submit'   => array('basic_aggr_s3_force_refresh_drop'),
  );
  return $form;
}
/**
 * S3 Refresh form drop all.
 *
 * @param array $form
 *   Current for structure.
 * @param array $form_state
 *   Current form state.
 */
function basic_aggr_s3_force_refresh_drop($form, &$form_state) {
  db_truncate('basic_aggr_s3_files')->execute();
  drupal_set_message('Table cleared');
}
/**
 * S3 Refresh form refresh all.
 *
 * @param array $form
 *   Current for structure.
 * @param array $form_state
 *   Current form state.
 */
function basic_aggr_s3_force_refresh_all($form, &$form_state) {
  $batch = array(
    'title' => t('Map Assets to productions'),
    'operations' => array(
      array('_basic_aggr_s3_build_parse_modules', array()),
      array('_basic_aggr_s3_build_parse_themes', array()),
      array('_basic_aggr_s3_build_parse_folders', array(FALSE)),
    ),
    'finished' => 'basic_aggr_s3_force_refresh_batch_fin',
    'file' => drupal_get_path('module', 'basic_aggr_s3') . '/basic_aggr_s3.batch.inc',
  );
  batch_set($batch);
}
/**
 * S3 Refresh form upload new files.
 *
 * @param array $form
 *   Current for structure.
 * @param array $form_state
 *   Current form state.
 */
function basic_aggr_s3_force_refresh_new($form, &$form_state) {
  $batch = array(
    'title' => t('Map Assets to productions'),
    'operations' => array(
      array('_basic_aggr_s3_build_parse_modules', array()),
      array('_basic_aggr_s3_build_parse_themes', array()),
      array('_basic_aggr_s3_build_parse_folders', array(TRUE)),
    ),
    'finished' => 'basic_aggr_s3_force_refresh_batch_fin',
    'file' => drupal_get_path('module', 'basic_aggr_s3') . '/basic_aggr_s3.batch.inc',
  );
  batch_set($batch);
}
/**
 * Batch process end function.
 *
 * @param boolean $success
 *   Flag to say if it was successful.
 * @param array $results
 *   Results passed back from batch functions.
 * @param string $operations
 *   Not sure.
 */
function basic_aggr_s3_force_refresh_batch_fin($success, $results, $operations) {
  if ($success) {
    drupal_set_message('Files upload to S3 bucket.');
  }
}
/**
 * Batch Process to collect all the module folders.
 * @access private
 *
 * @param array $context
 *   Current batch config.
 */
function _basic_aggr_s3_build_parse_modules(&$context) {
  if (empty($context['results'])) {
    $context['results'] = array(
      'folders' => array(),
    );
  }
  basic_aggr_s3_build_parse_modules($context['results']['folders']);
}
/**
 * Batch Process to collect all the theme folders.
 * @access private
 *
 * @param array $context
 *   Current batch config.
 */
function _basic_aggr_s3_build_parse_themes(&$context) {
  if (empty($context['results'])) {
    $context['results'] = array(
      'folders' => array(),
    );
  }
  basic_aggr_s3_build_parse_themes($context['results']['folders']);
}
/**
 * Batch Process to parse folders for files to upload.
 * @access private
 *
 * @param boolean $cache
 *   Flag to turn on caching to stop with going through all files.
 * @param array $context
 *   Current batch config.
 */
function _basic_aggr_s3_build_parse_folders($cache, &$context) {
  if (empty($context['sandbox'])) {
    $context['sandbox']['mask'] = variable_get(
      'basic_aggr_s3_mask',
      '/\.png|\.jpe?g|\.gif|\.ico|\.woff|\.svg|\.ttf|\.otf\.eot$/'
    );
    if ($cache) {
      $result = db_select('basic_aggr_s3_files', 'c')
        ->fields('c')
        ->execute()
        ->fetchAllAssoc('path');
      $context['sandbox']['cache'] = $result;
    }
    else {
      $context['sandbox']['cache'] = array();
    }
    $context['sandbox']['max'] = count($context['results']['folders']);
    $context['sandbox']['folders'] = $context['results']['folders'];
  }
  $options = array(
    'key' => 'uri',
    'recurse' => TRUE,
  );
  $folder = array_shift($context['sandbox']['folders']);
  $files = file_scan_directory($folder, $context['sandbox']['mask'], $options);
  foreach (array_keys($files) AS $file) {
    basic_aggr_s3_build_file_upload($file, $context['sandbox']['cache']);
  }
  $context['message'] = t(
    'Processing "@one" out of "@two"',
    array(
      '@one' => count($context['sandbox']['folders']),
      '@two' => $context['sandbox']['max'],
    )
  );
  if (count($context['sandbox']['folders']) > 0) {
    $context['finished']
      = ($context['sandbox']['max'] - count($context['sandbox']['folders']))
            / $context['sandbox']['max'];
  }
}
