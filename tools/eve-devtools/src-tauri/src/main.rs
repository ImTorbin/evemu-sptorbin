// Prevents additional console window on Windows in release, DO NOT REMOVE!!
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use serde::{Deserialize, Serialize};

/// Connection profile stored in the OS keychain.  The app UI drives these
/// through the `save_profile`, `load_profiles`, `delete_profile` commands so
/// the Rust side never has to parse EVEmu-specific data.
#[derive(Clone, Debug, Serialize, Deserialize)]
struct Profile {
    name: String,
    base_url: String,
    // Optional bootstrap token; never stored in plain localStorage.
    admin_token: Option<String>,
}

const KEYRING_SERVICE: &str = "dev.evemu.devtools";

#[tauri::command]
async fn save_profile(profile: Profile) -> Result<(), String> {
    let entry = keyring::Entry::new(KEYRING_SERVICE, &profile.name).map_err(|e| e.to_string())?;
    let json = serde_json::to_string(&profile).map_err(|e| e.to_string())?;
    entry.set_password(&json).map_err(|e| e.to_string())?;
    Ok(())
}

#[tauri::command]
async fn load_profile(name: String) -> Result<Option<Profile>, String> {
    let entry = keyring::Entry::new(KEYRING_SERVICE, &name).map_err(|e| e.to_string())?;
    match entry.get_password() {
        Ok(v) => {
            let p: Profile = serde_json::from_str(&v).map_err(|e| e.to_string())?;
            Ok(Some(p))
        }
        Err(keyring::Error::NoEntry) => Ok(None),
        Err(e) => Err(e.to_string()),
    }
}

#[tauri::command]
async fn delete_profile(name: String) -> Result<(), String> {
    let entry = keyring::Entry::new(KEYRING_SERVICE, &name).map_err(|e| e.to_string())?;
    if let Err(e) = entry.delete_credential() {
        if !matches!(e, keyring::Error::NoEntry) {
            return Err(e.to_string());
        }
    }
    Ok(())
}

/// Thin pass-through for HTTPS requests to admin API hosts that serve
/// self-signed certificates.  The web-view's fetch refuses those; routing
/// through reqwest (with native root CAs available) lets operators run the
/// tool against a local dev server without importing certs system-wide.
#[tauri::command]
async fn passthrough_fetch(
    method: String,
    url: String,
    token: Option<String>,
    body: Option<String>,
    content_type: Option<String>,
) -> Result<PassthroughResponse, String> {
    let client = reqwest::Client::builder()
        .danger_accept_invalid_certs(true)
        .build()
        .map_err(|e| e.to_string())?;

    let mut req = client.request(
        reqwest::Method::from_bytes(method.as_bytes()).map_err(|e| e.to_string())?,
        &url,
    );
    if let Some(t) = token {
        req = req.bearer_auth(t);
    }
    if let Some(b) = body {
        req = req.header(
            "content-type",
            content_type.unwrap_or_else(|| "application/json".into()),
        );
        req = req.body(b);
    }
    let res = req.send().await.map_err(|e| e.to_string())?;
    let status = res.status().as_u16();
    let text = res.text().await.unwrap_or_default();
    Ok(PassthroughResponse { status, body: text })
}

#[derive(Serialize)]
struct PassthroughResponse {
    status: u16,
    body: String,
}

fn main() {
    tauri::Builder::default()
        .plugin(tauri_plugin_store::Builder::default().build())
        .invoke_handler(tauri::generate_handler![
            save_profile,
            load_profile,
            delete_profile,
            passthrough_fetch,
        ])
        .run(tauri::generate_context!())
        .expect("error while running EVEmu DevTools");
}
