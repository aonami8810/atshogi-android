# Fix NDK [CXX1101] and Consolidate Build Files

The project currently has two build files in the `:app` module: `build.gradle` (Groovy) and `build.gradle.kts` (Kotlin DSL). This redundancy can cause build issues and confusion. Additionally, the build is failing because it's detecting a corrupted NDK installation at `27.0.12077973`.

## Proposed Changes

### Build Configuration

#### [MODIFY] [app/build.gradle.kts](file:///C:/VS/Workspace/atshogi-android/app/build.gradle.kts)
- Merge missing configurations from `app/build.gradle`:
    - Add `testInstrumentationRunner`.
    - Enable `dataBinding`.
    - Update `cmake` flags to include `-ffast-math`.
    - Ensure `ndkVersion` is set to `25.1.8937393`.

#### [DELETE] [app/build.gradle](file:///C:/VS/Workspace/atshogi-android/app/build.gradle)
- Remove the redundant Groovy build file.

## Verification Plan

### Automated Tests
- Run `gradlew clean assembleDebug` to verify the build process completes without the NDK error.

### Manual Verification
- Perform a Gradle Sync in Android Studio.
- If the error persists, it may be necessary to manually delete the corrupted folder at `C:\Users\Admin\AppData\Local\Android\Sdk\ndk\27.0.12077973`.
