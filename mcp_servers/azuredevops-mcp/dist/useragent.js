// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
class UserAgentComposer {
    _userAgent;
    _mcpClientInfoAppended;
    constructor(packageVersion) {
        this._userAgent = `AzureDevOps.MCP/${packageVersion} (local)`;
        this._mcpClientInfoAppended = false;
    }
    get userAgent() {
        return this._userAgent;
    }
    appendMcpClientInfo(info) {
        if (!this._mcpClientInfoAppended && info && info.name && info.version) {
            this._userAgent += ` ${info.name}/${info.version}`;
            this._mcpClientInfoAppended = true;
        }
    }
}
export { UserAgentComposer };
