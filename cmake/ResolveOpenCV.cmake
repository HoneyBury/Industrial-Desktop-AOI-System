function(aoi_resolve_opencv third_party_root)
  if(OpenCV_DIR)
    message(STATUS "Using user-provided OpenCV_DIR: ${OpenCV_DIR}")
    return()
  endif()

  set(_opencv_candidate_roots
    "${third_party_root}"
    "${third_party_root}/install"
    "${third_party_root}/build"
    "${third_party_root}/macos-arm64"
    "${third_party_root}/macos-x64"
    "${third_party_root}/windows-x64"
    "${third_party_root}/linux-x64")

  if(APPLE)
    list(APPEND _opencv_candidate_roots
      "/opt/homebrew/opt/opencv"
      "/usr/local/opt/opencv")
  endif()

  foreach(_root IN LISTS _opencv_candidate_roots)
    if(NOT EXISTS "${_root}")
      continue()
    endif()

    set(_candidate_dirs
      "${_root}"
      "${_root}/lib/cmake/opencv4"
      "${_root}/lib64/cmake/opencv4"
      "${_root}/cmake"
      "${_root}/build"
      "${_root}/build/lib")

    foreach(_candidate IN LISTS _candidate_dirs)
      if(EXISTS "${_candidate}/OpenCVConfig.cmake")
        set(OpenCV_DIR "${_candidate}" CACHE PATH "Resolved OpenCV config directory" FORCE)
        message(STATUS "Resolved OpenCV_DIR from project third_party: ${OpenCV_DIR}")
        return()
      endif()
    endforeach()
  endforeach()
endfunction()
