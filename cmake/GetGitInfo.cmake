# Copyright (C) 2022 xent
# Project is distributed under the terms of the GNU General Public License v3.0

function(git_commit_count _sw_commit_count)
    set(${_sw_commit_count} 0 PARENT_SCOPE)
endfunction()

function(git_hash _sw_hash)
    set(${_sw_hash} "00000000" PARENT_SCOPE)
endfunction()

function(git_version _sw_major _sw_minor)
    set(${_sw_major} 1 PARENT_SCOPE)
    set(${_sw_minor} 0 PARENT_SCOPE)
endfunction()
