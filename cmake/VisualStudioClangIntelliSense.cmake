# CMake passes the language standard to clang-cl correctly, but Visual
# Studio's design-time build reads ClCompile.LanguageStandard instead. Generate
# that metadata from CMake's selected standard; normal builds are unchanged.
if(DEFINED CMAKE_CXX_STANDARD)
    set(DICOM_EDITOR_VS_LANGUAGE_STANDARD "stdcpp${CMAKE_CXX_STANDARD}")
    if(
        DEFINED CMAKE_CXX_STANDARD_LATEST
        AND CMAKE_CXX_STANDARD GREATER_EQUAL CMAKE_CXX_STANDARD_LATEST
    )
        set(DICOM_EDITOR_VS_LANGUAGE_STANDARD "stdcpplatest")
    endif()

    string(CONFIGURE [=[
<Project>
  <ItemDefinitionGroup Condition="'$(PlatformToolset)' == 'ClangCL' and '$(DesignTimeBuild)' == 'true'">
    <ClCompile>
      <LanguageStandard>@DICOM_EDITOR_VS_LANGUAGE_STANDARD@</LanguageStandard>
    </ClCompile>
  </ItemDefinitionGroup>
</Project>
]=] _dicom_editor_directory_build_props @ONLY)
    file(GENERATE
        OUTPUT "${CMAKE_BINARY_DIR}/Directory.Build.props"
        CONTENT "${_dicom_editor_directory_build_props}")
endif()
