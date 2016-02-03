<?php
/**
 * This file is part of OpenMediaVault.
 *
 * @license   http://www.gnu.org/licenses/gpl.html GPL Version 3
 * @author    Volker Theile <volker.theile@openmediavault.org>
 * @author    OpenMediaVault Plugin Developers <plugins@omv-extras.org>
 * @copyright Copyright (c) 2009-2015 Volker Theile
 * @copyright Copyright (c) 2015 OpenMediaVault Plugin Developers
 *
 * OpenMediaVault is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * OpenMediaVault is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenMediaVault. If not, see <http://www.gnu.org/licenses/>.
 */

require_once "openmediavault/object.inc";
require_once "openmediavault/config.inc";
require_once "openmediavault/error.inc";
require_once "openmediavault/rpcservice.inc";
require_once "openmediavault/notify.inc";
require_once "openmediavault/util.inc";

class OMVRpcServiceMpd extends OMVRpcServiceAbstract
{
    private $rpcServiceName = "MPD";
    private $moduleName = "mpd";

    private $configXPath = "/config/services/mpd";
    private $audioOutputsXPath = "/config/services/mpd/audiooutputs";
    private $individualAudioOutputXPath = "/config/services/mpd/audiooutputs/audiooutput";

    private $eventMessagePath = "org.openmediavault.services.mpd";
    private $audioOutputEventMessagePath = "org.openmediavault.services.mpd.audiooutputs.audiooutput";


    /**
     * Get the name of the RPC service.
     *
     * @return string
     */
    public function getName()
    {
        return $this->rpcServiceName;
    }


    /**
     * Get an individual audio output configuration XPath by UUID
     *
     * @return string
     */
    private function getAudioOutputXPathByUuid($uuid)
    {
        return sprintf(
            "%s[uuid='%s']",
            $this->individualAudioOutputXPath,
            $uuid
        );
    }

    /**
     * Get a system shared folder configuration XPath by UUID
     *
     * @return string
     */
    private function getSharedFolderXPathByUuid($uuid)
    {
        return sprintf("/config/system/shares/sharedfolder[uuid='%s']", $uuid);
    }


    /**
     * Initialize the RPC service.
     *
     * @return void
     */
    public function initialize()
    {
      $this->registerMethod("getSettings");
      $this->registerMethod("setSettings");
      $this->registerMethod("getAudioOutputList");
      $this->registerMethod("getAudioOutput");
      $this->registerMethod("setAudioOutput");
      $this->registerMethod("deleteAudioOutput");
      $this->registerMethod("doUpdateDb");
    }

    /**
     * Get the settings of the service.
     *
     * @param array $params The method paramaters.
     * @param array $context The context of the caller.
     *
     * @throws E_CONFIG_GET_OBJECT_FAILED
     *
     * @return mixed
     */
    public function getSettings($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        // Get configuration object
        $object = $xmlConfig->get($this->configXPath);

        if (is_null($object)) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->configXPath
            );
        }

        // Modify result data
        $object["enable"]                            = boolval($object["enable"]);
        $object["enable_music_dir"]                  = boolval($object["enable_music_dir"]);
        $object["enable_playlist_dir"]               = boolval($object["enable_playlist_dir"]);
        $object["port"]                              = intval($object["port"]);
      	$object["auto_update"]                       = boolval($object["auto_update"]);
      	$object["follow_outside_symlinks"]           = boolval($object["follow_outside_symlinks"]);
      	$object["follow_inside_symlinks"]            = boolval($object["follow_inside_symlinks"]);
      	$object["save_absolute_paths_in_playlists"]  = boolval($object["save_absolute_paths_in_playlists"]);
      	$object["password_required"]                 = boolval($object["password_required"]);
        $object["enable_replaygain"]                 = intval($object["enable_replaygain"]);
      	$object["replaygain_preamp"]                 = intval($object["replaygain_preamp"]);
      	$object["replaygain_missing_preamp"]         = intval($object["replaygain_missing_preamp"]);
      	$object["replaygain_limit"]                  = boolval($object["replaygain_limit"]);
      	$object["volume_normalization"]              = boolval($object["volume_normalization"]);
      	$object["gapless_mp3_playback"]              = boolval($object["gapless_mp3_playback"]);
      	$object["buffer_before_play"]                = intval($object["buffer_before_play"]);
      	$object["restore_paused"]                    = boolval($object["restore_paused"]);
        unset($object['audiooutputs']);

        return $object;
    }

    /**
     * Set the settings of the service.
     *
     * @param array $params The method paramaters.
     * @param array $context The context of the caller.
     *
     * @throws E_CONFIG_GET_OBJECT_FAILED
     * @throws E_CONFIG_SET_OBJECT_FAILED
     *
     * @return mixed
     */
    public function setSettings($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams(
            $params,
            '{
                "type": "object",
                "properties": {
                    "enable":                             { "type": "boolean" },
                    "bind_to_address":                    { "type": "string" },
                    "port":                               { "type": "integer", "minimum":1, "maximum":65535 },
                    "log_level":                          { "type": "string" },
                    "enable_music_dir":                   { "type": "boolean" },
                    "music_directory":                    { '.$GLOBALS['OMV_JSONSCHEMA_UUID_NONE'].' },
                    "auto_update":                        { "type": "boolean" },
                    "follow_outside_symlinks":            { "type": "boolean" },
                    "follow_inside_symlinks":             { "type": "boolean" },
                    "enable_playlist_dir":                { "type": "boolean" },
                    "playlist_directory":                 { '.$GLOBALS['OMV_JSONSCHEMA_UUID_NONE'].' },
                    "save_absolute_paths_in_playlists":   { "type": "boolean" },
                    "password_required":                  { "type": "boolean" },
                    "password":                           { "type": "string" },
                    "enable_replaygain":                  { "type": "boolean" },
                    "replaygain":                         { "type": "string" },
                    "replaygain_preamp":                  { "type": "integer", "minimum":-15, "maximum":15 },
                    "replaygain_missing_preamp":          { "type": "integer", "minimum":-15, "maximum":15 },
                    "replaygain_limit":                   { "type": "boolean" },
                    "volume_normalization":               { "type": "boolean" },
                    "gapless_mp3_playback":               { "type": "boolean" },
                    "buffer_before_play":                 { "type": "integer", "minimum":0, "maximum":100 },
                    "restore_paused":                     { "type": "boolean" },
                    "extra_options":                      { "type": "string" }
                }
            }'
        );

        // Get the existing configuration object.
        $oldObject = $xmlConfig->get($this->configXPath);

        if (is_null($oldObject))
        {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->configXPath
            );
        }

        // Prepare configuration data
        $object = array(
            "enable"                            => boolval($params["enable"]),
            "bind_to_address"                   => $params['bind_to_address'],
            "port"                              => $params['port'],
            "log_level"                         => $params['log_level'],
            "enable_music_dir"                  => boolval($params["enable_music_dir"]),
            "music_directory"                   => $params['music_directory'],
            "auto_update"                       => boolval($params["auto_update"]),
            "follow_outside_symlinks"           => boolval($params["follow_outside_symlinks"]),
            "follow_inside_symlinks"            => boolval($params["follow_inside_symlinks"]),
            "enable_playlist_dir"               => boolval($params["enable_playlist_dir"]),
            "playlist_directory"                => $params['playlist_directory'],
            "save_absolute_paths_in_playlists"  => boolval($params["save_absolute_paths_in_playlists"]),
            "password_required"                 => boolval($params["password_required"]),
            "password"                          => $params['password'],
            "enable_replaygain"                 => boolval($params["enable_replaygain"]),
            "replaygain"                        => $params['replaygain'],
            "replaygain_preamp"                 => $params['replaygain_preamp'],
            "replaygain_missing_preamp"         => $params['replaygain_missing_preamp'],
            "replaygain_limit"                  => boolval($params["replaygain_limit"]),
            "volume_normalization"              => boolval($params["volume_normalization"]),
            "gapless_mp3_playback"              => boolval($params["gapless_mp3_playback"]),
            "buffer_before_play"                => $params['buffer_before_play'],
            "restore_paused"                    => boolval($params["restore_paused"]),
            "extra_options"                     => $params['extra_options'],
            "audiooutputs"                      => isset($oldObject["audiooutputs"]) ? $oldObject["audiooutputs"] : array()
        );

        // Set configuration object
        if (false === $xmlConfig->replace($this->configXPath, $object)) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_SET_OBJECT_FAILED,
                $this->configXPath
            );
        }

        // Notify configuration changes
        $dispatcher = &OMVNotifyDispatcher::getInstance();
        $dispatcher->notify(
            OMV_NOTIFY_MODIFY,
            $this->eventMessagePath,
            $object
        );

        return $object;
    }

    /**
     * Get a list of defined audio outputs
     *
     * @param array $params The method paramaters.
     * @param array $context The context of the caller.
     *
     * @throws E_CONFIG_GET_OBJECT_FAILED
     *
     * @return array List of defined audio outputs
     */
    public function getAudioOutputList($params, $context)
    {
      global $xmlConfig;

      // Validate the RPC caller context.
      $this->validateMethodContext(
          $context,
          array( "role" => OMV_ROLE_ADMINISTRATOR )
      );

      // Validate the parameters of the RPC service method.
      $this->validateMethodParams(
          $params,
          '{
              "type"       : "object",
              "properties" : {
                  "start"     : { "type" : "integer" },
                  "limit"     : { ' . $GLOBALS["OMV_JSONSCHEMA_COUNTFIELD"] . ' },
                  "sortfield" : { ' . $GLOBALS["OMV_JSONSCHEMA_SORTFIELD"] . ' },
                  "sortdir"   : { ' . $GLOBALS["OMV_JSONSCHEMA_SORTDIR"] . ' }
              }
          }'
      );

      // Get the configuration object.
      $audiooutputs = $xmlConfig->getList($this->individualAudioOutputXPath);

      if (is_null($audiooutputs)) {
          throw new OMVException(
              OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
              $this->individualAudioOutputXPath
          );
      }

      // Filter the result.
      return $this->applyFilter(
          $audiooutputs,
          $params["start"],
          $params["limit"],
          $params["sortfield"],
          $params["sortdir"]
      );
    }

    /**
     * Get an individual defined audio output
     *
     * @param array $params The method paramaters.
     * @param array $context The context of the caller.
     *
     * @throws E_CONFIG_GET_OBJECT_FAILED
     *
     * @return string XML config for audio output
     */
    public function getAudioOutput($params, $context)
    {
      global $xmlConfig;

      // Validate the RPC caller context.
      $this->validateMethodContext($context, array(
          "role" => OMV_ROLE_ADMINISTRATOR
      ));

      // Validate the parameters of the RPC service method.
      $this->validateMethodParams(
          $params,
          '{
              "type"       : "object",
              "properties" : {
                  "uuid" : { ' . $GLOBALS["OMV_JSONSCHEMA_UUID"] . ' }
              }
          }'
      );

      // Get the configuration object.
      $xPath = $this->getAudioOutputXPathByUuid($params["uuid"]);
      $ao = $xmlConfig->get($xPath);

      if (is_null($ao))
      {
          throw new OMVException(
              OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
              $xPath
          );
      }

      return $ao;
    }

    /**
     * Set an audio output
     *
     * @param array $params The method paramaters.
     * @param array $context The context of the caller.
     *
     * @throws E_CONFIG_SET_OBJECT_FAILED
     *
     * @return array Configuration object for the audio output
     */
    public function setAudioOutput($params, $context)
    {
      global $xmlConfig;

      // Validate the RPC caller context.
      $this->validateMethodContext(
          $context,
          array( "role" => OMV_ROLE_ADMINISTRATOR )
      );

      // Validate the parameters of the RPC service method.
      $this->validateMethodParams(
          $params,
          '{
              "type"       : "object",
              "properties" : {
                  "enabled"             : { "type" : "boolean" },
                  "name"                : { "type" : "string" },
                  "type"                : { "type" : "string" },
                  "format"              : { "type" : "string" },
                  "tags"                : { "type" : "boolean" },
                  "always_on"           : { "type" : "boolean" },
                  "mixer_type"          : { "type" : "string" },
                  "replaygain_handler"  : { "type" : "string" },
                  "extra_options"       : { "type" : "string" },
                  "uuid"                : { ' . $GLOBALS["OMV_JSONSCHEMA_UUID_UNDEFINED"] . ' }
              }
          }'
      );

      // Prepare the configuration object.
      $ao = array(
          "uuid"                => ($params["uuid"] == $GLOBALS["OMV_UUID_UNDEFINED"]) ? OMVUtil::uuid() : $params["uuid"],
          "name"                => $params["name"],
          "type"                => $params["type"],
          "format"              => $params["format"],
          "enabled"             => boolval($params["enabled"]),
          "tags"                => boolval($params["tags"]),
          "always_on"           => boolval($params["always_on"]),
          "mixer_type"          => $params["mixer_type"],
          "replaygain_handler"  => $params["replaygain_handler"],
          "extra_options"       => $params["extra_options"]
      );

      // Set the configuration object.
      $success = false;

      if ($params["uuid"] == $GLOBALS["OMV_UUID_UNDEFINED"])
      {
          // Check for unique name
          $xPath = sprintf(
              "%s[name='%s']",
              $this->individualAudioOutputXPath,
              $params["name"]
          );

          if ($xmlConfig->exists($xPath) === true)
              throw new OMVException(
                  OMVErrorMsg::E_CONFIG_OBJECT_UNIQUENESS,
                    gettext("The configured name is already used by another audio output.")
              );

          // Append the configuration object.
          $success = $xmlConfig->set($this->audioOutputsXPath, array("audiooutput" => $ao));
      }
      else
      {
          // Update the existing configuration object.
          $success = $xmlConfig->replace($this->getAudioOutputXPathByUuid($params["uuid"]),$ao);
      }

      if (!$success)
      {
          throw new OMVException(
              OMVErrorMsg::E_CONFIG_SET_OBJECT_FAILED
          );
      }

      // Notify configuration changes.
      $dispatcher = &OMVNotifyDispatcher::getInstance();

      $dispatcher->notify(
          ($params["uuid"] == $GLOBALS["OMV_UUID_UNDEFINED"]) ? OMV_NOTIFY_CREATE : OMV_NOTIFY_MODIFY,
          $this->audioOutputEventMessagePath,
          $ao
      );

      // Return the configuration object.
      return $ao;
    }

    /**
     * Delete an audio output
     *
     * @param array $params The method paramaters.
     * @param array $context The context of the caller.
     *
     * @throws E_CONFIG_SET_OBJECT_FAILED
     *
     * @return array Configuration object for the audio output
     */
    public function deleteAudioOutput($params, $context)
    {
      // Validate the RPC caller context.
      $this->validateMethodContext($context, array(
          "role" => OMV_ROLE_ADMINISTRATOR
      ));

      // Validate the parameters of the RPC service method.
      $this->validateMethodParams(
          $params,
          '{
              "type"       : "object",
              "properties" : {
                  "uuid" : { ' . $GLOBALS["OMV_JSONSCHEMA_UUID"] . ' }
              }
          }'
      );

      // Delete the configuration object.
      $ao = $this->deleteConfigObjectByPath(
          $this->getAudioOutputXPathByUuid($params["uuid"]),
          $this->audioOutputEventMessagePath
      );

      // Return the deleted configuration object.
      return $ao;
    }

    /**
     * Trigger an update of MPD's database (via mpc)
     *
     * @param array $params The method paramaters.
     * @param array $context The context of the caller.
     *
     * @throws E_CONFIG_GET_OBJECT_FAILED
     * @throws E_EXEC_FAILED
     * @throws E_CONFIG_DIRTY
     * @throws E_MISC_FAILURE
     *
     * @return None
     */
    public function doUpdateDb($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext(
            $context,
            array( "role" => OMV_ROLE_ADMINISTRATOR )
        );

        if ($this->isModuleDirty($this->moduleName)) {
                throw new OMVException(OMVErrorMsg::E_CONFIG_DIRTY);
        }

        $object = $xmlConfig->get($this->configXPath);

        if (is_null($object))
        {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->configXPath
            );
        }

        if (!boolval($object["enable"]))
        {
          throw new OMVException(
            OMVErrorMsg::E_MISC_FAILURE,
            "MPD service is disabled."
          );
        }

        foreach ( array("enable_music_dir", "music_directory") as
          $keyk => $keyv ) {
            if (empty($object[$keyv])) {
              throw new OMVException(
                OMVErrorMsg::E_MISC_FAILURE,
                "MPD music directory is not configured.");
            }
        }

        $cmd = "mpc update 2>&1";
        OMVUtil::exec($cmd, $output, $result);
        if ($result !== 0) {
          throw new OMVException(
            OMVErrorMsg::E_EXEC_FAILED,
            $cmd, implode("\n", $output)
          );
        }
    }

}

// Register the RPC service.
$rpcServiceMgr = &OMVRpcServiceMgr::getInstance();
$rpcServiceMgr->registerService(new OMVRpcServiceMpd());
